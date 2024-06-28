#!/usr/bin/env python3

"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import threading
import time
from queue import Empty
from typing import Dict, Callable, Any, Tuple

import zmq

from .broadcastmanager import CHIRPBroadcaster, chirp_callback, DiscoveredService
from .chirp import CHIRPServiceIdentifier, get_uuid
from .cscp import CommandTransmitter
from .error import debug_log
from .fsm import SatelliteState
from .satellite import Satellite
from .base import EPILOG, ConstellationArgumentParser, setup_cli_logging
from .commandmanager import get_cscp_commands
from .configuration import load_config, flatten_config
from .heartbeatchecker import HeartbeatChecker


class SatelliteClassCommLink:
    """A link to a Satellite Class."""

    def __init__(self, name: str):
        self._class_name = name

    def __str__(self) -> str:
        """Convert to class name."""
        return self._class_name


class SatelliteCommLink(SatelliteClassCommLink):
    """A link to a Satellite."""

    def __init__(self, name: str, cls: str):
        self._name = name
        self._uuid = str(get_uuid(f"{cls}.{name}"))
        super().__init__(cls)

    def __str__(self) -> str:
        """Convert to canonical name."""
        return f"{self._class_name}.{self._name}"


class SatelliteArray:
    """Provide object-oriented control of connected Satellites."""

    def __init__(
        self,
        group: str,
        handler: Callable[[str, str, str, Any], Tuple[str, Any, dict[str, Any] | None]],
    ):
        self.group = group
        self._handler = handler
        # initialize with the commands known to any CSCP Satellite
        self._add_cmds(self, self._handler, get_cscp_commands(Satellite))
        self._satellites: dict[str, SatelliteCommLink] = {}

    @property
    def satellites(self) -> dict[str, SatelliteCommLink]:
        """Return the dict of Satellites names and their SatelliteCommLink."""
        return self._satellites

    def get_satellite(self, sat_class: str, sat_name: str) -> SatelliteCommLink:
        """Return a link to a Satellite given by its class and name."""
        return self._satellites[f"{sat_class}.{sat_name}"]

    def _add_class(self, name: str, commands: dict[str, Any]) -> SatelliteClassCommLink:
        """Add a new class to the array."""
        try:
            cl: SatelliteClassCommLink = getattr(self, name)
            return cl
        except AttributeError:
            pass
        # add attributes now
        cl = SatelliteClassCommLink(name)
        self._add_cmds(cl, self._handler, commands)
        setattr(self, name, cl)
        return cl

    def _add_satellite(
        self, name: str, cls: str, commands: dict[str, str]
    ) -> SatelliteCommLink:
        """Add a new Satellite."""
        try:
            cl: SatelliteClassCommLink = getattr(self, cls)
        except AttributeError:
            cl = self._add_class(cls, commands)
        sat: SatelliteCommLink = SatelliteCommLink(name, cls)
        self._add_cmds(sat, self._handler, commands)
        setattr(cl, self._sanitize_name(name), sat)
        self._satellites[f"{cls}.{name}"] = sat
        return sat

    def _remove_satellite(self, uuid: str) -> None:
        """Remove a Satellite."""
        name, cls = self._get_name_from_uuid(uuid)
        # remove attribute
        delattr(getattr(self, cls), self._sanitize_name(name))
        # clear from dict
        self._satellites.pop(f"{cls}.{name}")

    def _get_name_from_uuid(self, uuid: str) -> Tuple[str, str]:
        s = [sat for sat in self._satellites.values() if sat._uuid == uuid]
        if not s:
            raise KeyError("No Satellite with that UUID known.")
        name = s[0]._name
        cls = s[0]._class_name
        return name, cls

    def _add_cmds(
        self,
        obj: Any,
        handler: Callable[[str, str, str, Any], Tuple[str, Any, dict[str, Any] | None]],
        cmds: dict[str, str],
    ) -> None:
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
            w.call.__func__.__doc__ = doc  # type: ignore[attr-defined]
            setattr(obj, cmd, w.call)

    def _sanitize_name(self, name: str) -> str:
        """Remove characters not suited for Python methods from names."""
        return name.replace("-", "_")


class CommandWrapper:
    """Class to wrap command calls.

    Allows to mimic the signature of the Satellite command being wrapped.
    """

    def __init__(
        self,
        handler: Callable[[str, str, str, Any], Tuple[str, Any, dict[str, Any] | None]],
        sat: str,
        satcls: str,
        cmd: str,
    ):
        """Initialize with fcn as a partial() call."""
        self.fcn = handler
        self.sat = sat
        self.satcls = satcls
        self.cmd = cmd

    def call(self, payload: Any = None) -> Tuple[str, Any, dict[str, Any] | None]:
        """Perform call. This doc string will be overwritten."""
        return self.fcn(sat=self.sat, satcls=self.satcls, cmd=self.cmd, payload=payload)  # type: ignore[call-arg]


class BaseController(CHIRPBroadcaster):
    """Simple controller class to send commands to a Constellation."""

    def __init__(self, group: str, **kwargs: Any) -> None:
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

        self._constellation = SatelliteArray(group, self.command)

        super()._add_com_thread()
        super()._start_com_threads()

        self._hb_checker = HeartbeatChecker(self._hb_failure)
        self._hb_checker.auto_recover = True

        # set up thread to handle incoming tasks (e.g. CHIRP discoveries)
        self._task_handler_event = threading.Event()
        self._task_handler_thread = threading.Thread(
            target=self._run_task_handler, daemon=True
        )
        self._task_handler_thread.start()

        # wait for threads to be ready
        time.sleep(0.2)
        self.request(CHIRPServiceIdentifier.CONTROL)
        self.request(CHIRPServiceIdentifier.HEARTBEAT)

    @property
    def states(self) -> dict[str, SatelliteState]:
        """Return an up-to-date dictionary of connected Satellite's status.

        Based on heartbeat information.

        """
        return self._hb_checker.states

    @property
    def constellation(self) -> SatelliteArray:
        """Returns the currently active SatelliteArray of controlled Satellites."""
        return self._constellation

    @debug_log
    @chirp_callback(CHIRPServiceIdentifier.CONTROL)
    def _add_satellite_callback(self, service: DiscoveredService) -> None:
        """Callback method connecting to satellite."""
        if not service.alive:
            self._remove_satellite(service)
        else:
            self._add_satellite(service)

    @debug_log
    @chirp_callback(CHIRPServiceIdentifier.HEARTBEAT)
    def _add_satellite_heatbeat(self, service: DiscoveredService) -> None:
        """Callback method registering satellite's heartbeat."""
        if not service.alive:
            return
        uuid = str(service.host_uuid)
        if uuid in self._transmitters:
            # we are controlling this satellite
            cls, name = self._uuid_lookup[uuid]
            canonical_name = f"{cls}.{name}"
            if not self._hb_checker.is_registered(canonical_name):
                self._hb_checker.register(
                    name=canonical_name,
                    address=f"tcp://{service.address}:{service.port}",
                )
                self._hb_checker.start(canonical_name)

    def _add_satellite(self, service: DiscoveredService) -> None:
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
            sat = self._constellation._add_satellite(name, cls, msg.payload)
            if sat._uuid != str(service.host_uuid):
                self.log.warning(
                    "UUIDs do not match: expected %s but received %s",
                    sat._uuid,
                    str(service.host_uuid),
                )
            uuid = str(service.host_uuid)
            self._uuid_lookup[uuid] = (cls, name)
            self._transmitters[uuid] = ct
            # check if heartbeat service for satellite is known already
            for hbservice in self.get_discovered(CHIRPServiceIdentifier.HEARTBEAT):
                if hbservice.host_uuid == service.host_uuid:
                    canonical_name = f"{cls}.{name}"
                    self._hb_checker.register(
                        name=canonical_name,
                        address=f"tcp://{hbservice.address}:{hbservice.port}",
                    )
                    self._hb_checker.start(canonical_name)
                    break
        except RuntimeError as e:
            self.log.error("Could not add Satellite %s: %s", service.host_uuid, repr(e))

    def _remove_satellite(self, service: DiscoveredService) -> None:
        name, cls = None, None
        # departure
        uuid = str(service.host_uuid)
        try:
            name, cls = self._constellation._get_name_from_uuid(uuid)
            self._constellation._remove_satellite(uuid)
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

    def _hb_failure(self, name: str, state: SatelliteState) -> None:
        """Callback for Satellites failing to send heartbeats."""
        # TODO add filter for only those Satellites that we control
        self.log.critical("%s has entered %s", name, state.name)

    def command(
        self, payload: Any = None, sat: str = "", satcls: str = "", cmd: str = ""
    ) -> Any:
        """Wrapper for _command_satellite function. Handle sending commands to all hosts"""
        targets = []
        # figure out whether to send command to Satellite, Class or whole Constellation
        if not sat and not satcls:
            targets = [sat._uuid for sat in self._constellation.satellites.values()]
            self.log.info(
                "Sending %s to all %s connected Satellites.", cmd, len(targets)
            )
        elif not sat:
            targets = [
                sat._uuid
                for sat in self._constellation.satellites.values()
                if sat._class_name == satcls
            ]
            self.log.info(
                "Sending %s to all %s connected Satellites of class %s.",
                cmd,
                len(targets),
                satcls,
            )
        else:
            assert satcls  # for typing
            assert sat  # for typing
            targets = [self._constellation.get_satellite(satcls, sat)._uuid]
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

    def _preprocess_payload(self, payload: Any, uuid: str, cmd: str) -> Any:
        """Pre-processes payload for specific commands."""
        if cmd == "initialize" or cmd == "reconfigure":
            # payload needs to be a flat dictionary, but we want to allow to
            # supply a full config -- flatten it here
            if any(isinstance(i, dict) for i in payload.values()):
                # have a nested dict
                cls, name = self._uuid_lookup[uuid]
                cfg = flatten_config(payload, cls, name)
                self.log.debug(
                    "Flattening and sending configuration for %s.%s", cls, name
                )
                return cfg
        return payload

    def _run_task_handler(self) -> None:
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

    def reentry(self) -> None:
        """Stop the controller."""
        self.log.info("Stopping controller.")
        if getattr(self, "_task_handler_event", None):
            self._task_handler_event.set()
        try:
            for _name, cmd_tm in self._transmitters.items():
                cmd_tm.socket.close()
        except Exception:
            # ignore errors; this avoids spurious error messages if e.g. the
            # initialization of the class fails
            pass
        try:
            self._hb_checker.stop()
        except Exception as e:
            self.log.warning(
                "Encountered problem shutting heartbeat checker down: %s", repr(e)
            )
        if getattr(self, "_task_handler_event", None):
            self._task_handler_thread.join()
        super().reentry()


def main(args: Any = None) -> None:
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
