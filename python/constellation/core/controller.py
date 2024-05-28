#!/usr/bin/env python3

"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import threading
import time
from queue import Empty
from typing import Dict

import zmq

from .broadcastmanager import CHIRPBroadcaster, chirp_callback, DiscoveredService
from .chirp import CHIRPServiceIdentifier, get_uuid

from .cscp import CommandTransmitter
from .error import debug_log
from .satellite import Satellite
from .base import EPILOG, ConstellationArgumentParser, setup_cli_logging
from .commandmanager import get_cscp_commands
from .configuration import load_config, flatten_config


class SatelliteArray:
    """Provide object-oriented control of connected Satellites."""

    def __init__(self, group: str, handler: callable):
        self.constellation = group
        self._handler = handler
        # initialize with the commands known to any CSCP Satellite
        self._add_cmds(self, self._handler, get_cscp_commands(Satellite))
        self._satellites: list(SatelliteCommLink) = []

    @property
    def satellites(self):
        """Return the list of known Satellite."""
        return self._satellites

    def _add_class(self, name: str, commands: dict[str]):
        """Add a new class to the array."""
        try:
            cl = getattr(self, name)
            return cl
        except AttributeError:
            pass
        # add attributes now
        cl = SatelliteClassCommLink(name)
        self._add_cmds(cl, self._handler, commands)
        setattr(self, name, cl)
        return cl

    def _add_satellite(self, name: str, cls: str, commands: dict[str]):
        """Add a new Satellite."""
        try:
            cl = getattr(self, cls)
        except AttributeError:
            cl = self._add_class(cls, commands)
        sat = SatelliteCommLink(name, cls)
        self._add_cmds(sat, self._handler, commands)
        setattr(cl, name, sat)
        self._satellites.append(sat)
        return sat

    def _remove_satellite(self, uuid: str):
        """Remove a Satellite."""
        name, cls = self._get_name_from_uuid(uuid)
        # remove attribute
        delattr(getattr(self, cls), name)
        # clear from list
        self._satellites = [sat for sat in self._satellites if sat._uuid != uuid]

    def _get_name_from_uuid(self, uuid: str):
        s = [sat for sat in self._satellites if sat._uuid == uuid]
        if not s:
            raise KeyError("No Satellite with that UUID known.")
        name = s[0]._name
        cls = s[0]._class_name
        return name, cls

    def _add_cmds(self, obj: any, handler: callable, cmds: dict[str]):
        try:
            sat = obj._name
        except AttributeError:
            sat = None
        try:
            satcls = obj._class_name
        except AttributeError:
            satcls = None
        for cmd, doc in cmds.items():
            w = CommandWrapper(handler, sat=sat, satcls=satcls, cmd=cmd)
            # add docstring
            w.call.__func__.__doc__ = doc
            setattr(obj, cmd, w.call)


class CommandWrapper:
    """Class to wrap command calls.

    Allows to mimic the signature of the Satellite command being wrapped.
    """

    def __init__(self, handler, sat, satcls, cmd):
        """Initialize with fcn as a partial() call."""
        self.fcn = handler
        self.sat = sat
        self.satcls = satcls
        self.cmd = cmd

    def call(self, payload=None):
        """Perform call. This doc string will be overwritten."""
        return self.fcn(sat=self.sat, satcls=self.satcls, cmd=self.cmd, payload=payload)


class SatelliteClassCommLink:
    """A link to a Satellite Class."""

    def __init__(self, name):
        self._class_name = name

    def __str__(self):
        """Convert to class name."""
        return self._class_name


class SatelliteCommLink(SatelliteClassCommLink):
    """A link to a Satellite."""

    def __init__(self, name, cls):
        self._name = name
        self._uuid = str(get_uuid(f"{cls}.{name}"))
        super().__init__(cls)

    def __str__(self):
        """Convert to canonical name."""
        return f"{self._class_name}.{self._name}"


class BaseController(CHIRPBroadcaster):
    """Simple controller class to send commands to a Constellation."""

    def __init__(self, group: str, **kwargs):
        """Initialize values.

        Arguments:
        - name ::  name of controller
        - group ::  group of controller
        - interface :: the interface to connect to
        """
        super().__init__(group=group, **kwargs)

        self._transmitters: Dict[str, CommandTransmitter] = {}
        # lookup table for uuids to (cls, name) tuple
        self._uuid_lookup: dict[str, tuple[str, str]] = {}

        self.constellation = SatelliteArray(group, self.command)

        super()._add_com_thread()
        super()._start_com_threads()

        # set up thread to handle incoming tasks (e.g. CHIRP discoveries)
        self._task_handler_event = threading.Event()
        self._task_handler_thread = threading.Thread(
            target=self._run_task_handler, daemon=True
        )
        self._task_handler_thread.start()

        # wait for threads to be ready
        time.sleep(0.2)
        self.request(CHIRPServiceIdentifier.CONTROL)

    @debug_log
    @chirp_callback(CHIRPServiceIdentifier.CONTROL)
    def _add_satellite_callback(self, service: DiscoveredService):
        """Callback method connecting to satellite."""
        if not service.alive:
            self._remove_satellite(service)
        else:
            self._add_satellite(service)

    def _add_satellite(self, service: DiscoveredService):
        self.log.debug("Adding Satellite %s", service)
        if str(service.host_uuid) in self._uuid_lookup.keys():
            self.log.error(
                "Satellite with name '%s.%s' already connected! Please ensure "
                "unique Satellite names or you will not be able to communicate with all!",
                *self._uuid_lookup[str(service.host_uuid)],
            )
        # create socket
        socket = self.context.socket(zmq.REQ)
        socket.connect("tcp://" + service.address + ":" + str(service.port))
        ct = CommandTransmitter(self.name, socket)
        self.log.debug(
            "Connecting to %s, address %s on port %s...",
            service.host_uuid,
            service.address,
            service.port,
        )
        try:
            # get list of commands
            msg = ct.request_get_response("get_commands")
            # get canonical name
            cls, name = msg.from_host.split(".", maxsplit=1)
            sat = self.constellation._add_satellite(name, cls, msg.payload)
            if sat._uuid != str(service.host_uuid):
                self.log.warning(
                    "UUIDs do not match: expected %s but received %s",
                    sat._uuid,
                    str(service.host_uuid),
                )
            uuid = str(service.host_uuid)
            self._uuid_lookup[uuid] = (cls, name)
            self._transmitters[uuid] = ct
        except RuntimeError as e:
            self.log.error("Could not add Satellite %s: %s", service.host_uuid, repr(e))

    def _remove_satellite(self, service: DiscoveredService):
        name, cls = None, None
        # departure
        uuid = str(service.host_uuid)
        try:
            name, cls = self.constellation._get_name_from_uuid(uuid)
            self.constellation._remove_satellite(uuid)
        except KeyError:
            pass
        self.log.debug(
            "Departure of %s, known as %s.%s",
            service.host_uuid,
            name,
            cls,
        )
        try:
            ct = self._transmitters[uuid]
            ct.socket.close()
            self._transmitters.pop(uuid)
            self._uuid_lookup.pop(uuid)
        except KeyError:
            pass

    def command(self, payload=None, sat=None, satcls=None, cmd=None):
        """Wrapper for _command_satellite function. Handle sending commands to all hosts"""
        targets = []
        # figure out whether to send command to Satellite, Class or whole Constellation
        if not sat and not satcls:
            targets = [sat._uuid for sat in self.constellation.satellites]
            self.log.info(
                "Sending %s to all %s connected Satellites.", cmd, len(targets)
            )
        elif not sat:
            targets = [
                sat._uuid
                for sat in self.constellation.satellites
                if sat._class_name == satcls
            ]
            self.log.info(
                "Sending %s to all %s connected Satellites of class %s.",
                cmd,
                len(targets),
                satcls,
            )
        else:
            targets = [getattr(getattr(self.constellation, satcls), sat)._uuid]
            self.log.info("Sending %s to Satellite %s.", cmd, targets[0])

        res = {}
        for target in targets:
            self.log.debug("Host %s send command %s...", target, cmd)
            # The payload to set of (known) command can be pre-processed
            # allowing using more complex objects as arguments and a more
            # convenient CLI user experience without impacting the protocol
            # specs. Here, we translate to what the protocol requires.
            p = self._preprocess_payload(payload, target, cmd)
            try:
                ret_msg = self._transmitters[target].request_get_response(
                    command=cmd,
                    payload=p,
                    meta=None,
                )
            except KeyError:
                self.log.error(
                    "Command %s failed for %s (%s.%s): No transmitter available",
                    cmd,
                    target,
                    satcls,
                    sat,
                )
                continue
            except RuntimeError as e:
                self.log.error(
                    "Command %s failed for %s (%s.%s): %s",
                    cmd,
                    target,
                    satcls,
                    sat,
                    repr(e),
                )
                continue
            self.log.debug(
                "%s responded: %s",
                ret_msg.from_host,
                ret_msg.msg,
            )
            if ret_msg.header_meta:
                self.log.debug("    header: %s", ret_msg.header_meta)
            if ret_msg.payload:
                self.log.debug("    payload: %s", ret_msg.payload)
            if sat:
                # simplify return value for single satellite
                res = {"msg": ret_msg.msg, "payload": ret_msg.payload}
            else:
                # append
                res[ret_msg.from_host] = {
                    "msg": ret_msg.msg,
                    "payload": ret_msg.payload,
                }
        return res

    def _preprocess_payload(self, payload: any, uuid: str, cmd: str) -> any:
        """Pre-processes payload for specific commands."""
        if cmd == "initialize":
            # payload needs to be a flat dictionary, but we want to allow to
            # supply a full config -- flatten it here
            if any(isinstance(i, dict) for i in payload.values()):
                # have a nested dict
                cls, name = self._uuid_lookup[uuid]
                self.log.debug("Flattening dictionary for %s.%s", cls, name)
                return flatten_config(payload, cls, name)
        return payload

    def _run_task_handler(self):
        """Event loop for task handler-routine"""
        while not self._task_handler_event.is_set():
            try:
                # blocking call but with timeout to prevent deadlocks
                task = self.task_queue.get(block=True, timeout=0.5)
                callback = task[0]
                args = task[1]
                try:
                    callback(*args)
                except Exception as e:
                    self.log.exception(e)
            except Empty:
                # nothing to process
                pass

    def reentry(self):
        """Stop the controller."""
        self.log.info("Stopping controller.")
        if getattr(self, "_task_handler_event", None):
            self._task_handler_event.set()
        for _name, cmd_tm in self._transmitters.items():
            cmd_tm.socket.close()
        if getattr(self, "_task_handler_event", None):
            self._task_handler_thread.join()
        super().reentry()


def main(args=None):
    """Start a Constellation CSCP controller.

    This Controller provides a command-line interface to the selected
    Constellation group via IPython terminal.

    """
    from IPython import embed

    parser = ConstellationArgumentParser(description=main.__doc__, epilog=EPILOG)
    parser.add_argument(
        "-c", "--config", type=str, help="Path to the TOML configuration file to load."
    )
    # set the default arguments
    parser.set_defaults(name="controller")
    # get a dict of the parsed arguments
    args = vars(parser.parse_args(args))

    # set up logging
    logger = setup_cli_logging(args["name"], args.pop("log_level"))

    cfg_file = args.pop("config")

    logger.debug("Starting up CLI Controller!")

    # start server with args
    ctrl = BaseController(**args)

    constellation = ctrl.constellation  # noqa

    print("\nWelcome to the Constellation CLI IPython Controller!\n")
    print(
        "You can interact with the discovered Satellites via the `constellation` array:"
    )
    print("          constellation.get_state()\n")
    print("To get help for any of its methods, call it with a question mark:")
    print("          constellation.get_state?\n")

    if cfg_file:
        cfg = load_config(cfg_file)  # noqa
        print(f"The configuration file '{cfg_file}' has been loaded into 'cfg'.\n")

    print("Happy hacking! :)\n")

    # start IPython console
    embed()


if __name__ == "__main__":
    main()
