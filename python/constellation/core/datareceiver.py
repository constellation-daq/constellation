#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

Base module for Constellation Satellites that receive data.
"""

import datetime
import pathlib
import sys
import threading

import zmq
from uuid import UUID
from functools import partial
from typing import Any, Tuple

from .broadcastmanager import chirp_callback, DiscoveredService
from .cdtp import CDTPMessage, CDTPMessageIdentifier, DataTransmitter
from .cmdp import MetricsType
from .chirp import CHIRPServiceIdentifier
from .commandmanager import cscp_requestable
from .cscp import CSCPMessage
from .fsm import SatelliteState
from .satellite import Satellite


class DataReceiver(Satellite):
    """Constellation Satellite which receives data via ZMQ."""

    def __init__(self, *args: Any, **kwargs: Any):
        # define our attributes
        self._pull_interfaces: dict[UUID, Tuple[str, int]] = {}
        self._pull_sockets: dict[UUID, zmq.Socket] = {}  # type: ignore[type-arg]
        self.poller: zmq.Poller | None = None
        self.run_identifier = ""
        # Tracker for which satellites have joined the current data run.
        self.active_satellites: list[str] = []
        # metrics
        self.receiver_stats: dict[str, int] = {}
        # initialize Satellite attributes
        super().__init__(*args, **kwargs)
        self.request(CHIRPServiceIdentifier.DATA)

    def do_initializing(self, config: dict[str, Any]) -> str:
        """Initialize and configure the satellite."""
        # what pattern to use for the file names?
        self.file_name_pattern = self.config.setdefault("file_name_pattern", "run_{run_identifier}_{date}.h5")
        # what directory to store files in?
        self.output_path = self.config.setdefault("output_path", "data")
        self._configure_monitoring(2.0)
        return "Configured DataReceiver"

    def do_launching(self) -> str:
        """Set up pull sockets to listen to incoming data."""
        # Set up the data poller which will monitor all ZMQ sockets
        self.poller = zmq.Poller()
        # TODO implement a filter based on configuration values
        for uuid, host in self._pull_interfaces.items():
            address, port = host
            self._add_socket(uuid, address, port)
        return "Established connections to data senders."

    def do_landing(self) -> str:
        """Close all open sockets."""
        for uuid in self._pull_interfaces.keys():
            self._remove_socket(uuid)
        self.poller = None
        return "Closed connections to data senders."

    def do_starting(self, run_identifier: str) -> str:
        self.active_satellites = []
        return "Started data receiving"

    def do_run(self, run_identifier: str) -> str:
        """Handle the data enqueued by the ZMQ Poller.

        The implementation of this method will have to monitor the
        `self._state_thread_evt` Event and perform all necessary steps for
        stopping the acquisition when `self._state_thread_evt.is_set()` is
        `True` as there will be *no* call to `do_stopping` in this class. This
        allows to e.g. wrap opening files in `with ...` or `try: ... finally:
        ...` clauses.

        This method will be executed in a separate thread by the underlying
        Satellite class. It therefore needs to monitor the self.stop_running
        Event and close itself down if the Event is set.

        """
        self.run_identifier = run_identifier
        filename = pathlib.Path(
            self.file_name_pattern.format(
                run_identifier=self.run_identifier,
                date=datetime.datetime.now().strftime("%Y-%m-%d-%H%M%S"),
            )
        )
        outfile = self._open_file(filename)
        last_msg = datetime.datetime.now()
        # keep the data collection alive for a few seconds after stopping
        keep_alive = datetime.datetime.now()
        transmitter = DataTransmitter("", None)
        self._reset_receiver_stats()
        try:
            # processing loop
            # assert for mypy static type analysis
            assert isinstance(self._state_thread_evt, threading.Event), "State thread Event not set up correctly"

            while not self._state_thread_evt.is_set() or ((datetime.datetime.now() - keep_alive).total_seconds() < 60):
                # refresh keep_alive timestamp
                if not self._state_thread_evt.is_set():
                    keep_alive = datetime.datetime.now()
                else:
                    if not self.active_satellites:
                        # no Satellites connected
                        self.log.info("All EOR received, stopping.")
                        break
                # request available data from zmq poller; timeout prevents
                # deadlock when stopping.
                assert isinstance(self.poller, zmq.Poller)
                sockets_ready = dict(self.poller.poll(timeout=250))

                for socket in sockets_ready.keys():
                    binmsg = socket.recv_multipart()
                    # NOTE below we determine the size of the list of (binary)
                    # strings, which is not exactly what went over the network
                    self.receiver_stats["nbytes"] += sys.getsizeof(binmsg)
                    self.receiver_stats["npackets"] += 1
                    try:
                        item = transmitter.decode(binmsg)
                    except Exception as e:
                        self.log.critical(
                            "Could not decode message '%s' due to exception: %s",
                            binmsg,
                            repr(e),
                        )
                        raise RuntimeError("Could not decode message") from e
                    try:
                        if item.msgtype == CDTPMessageIdentifier.BOR:
                            self.active_satellites.append(item.name)
                            self._write_BOR(outfile, item)
                        elif item.msgtype == CDTPMessageIdentifier.EOR:
                            self.active_satellites.remove(item.name)
                            self._write_EOR(outfile, item)
                        else:
                            self._write_data(outfile, item)
                    except Exception as e:
                        self.log.critical("Could not write message '%s' to file: %s", item, repr(e))
                        raise RuntimeError(f"Could not write message '{item}' to file") from e
                    if (datetime.datetime.now() - last_msg).total_seconds() > 2.0:
                        if self._state_thread_evt.is_set():
                            msg = "Finishing with"
                        else:
                            msg = "Processing"
                        self.log.status(
                            "%s data packet %s from %s",
                            msg,
                            item.sequence_number,
                            item.name,
                        )
                        last_msg = datetime.datetime.now()

        finally:
            self._close_file(outfile)
            if self.active_satellites:
                self.log.warning(
                    "Never received EOR from following Satellites: %s",
                    ", ".join(self.active_satellites),
                )
            self.active_satellites = []
        return f"Finished acquisition to {filename}"

    def _write_data(self, outfile: Any, item: CDTPMessage) -> None:
        """Write data to file"""
        raise NotImplementedError()

    def _write_EOR(self, outfile: Any, item: CDTPMessage) -> None:
        """Write EOR to file"""
        raise NotImplementedError()

    def _write_BOR(self, outfile: Any, item: CDTPMessage) -> None:
        """Write BOR to file"""
        raise NotImplementedError()

    def _open_file(self, filename: pathlib.Path) -> Any:
        """Return the filehandler"""
        raise NotImplementedError()

    def _close_file(self, outfile: Any) -> None:
        """Close the filehandler"""
        raise NotImplementedError()

    def fail_gracefully(self) -> str:
        """Method called when reaching 'ERROR' state."""
        for uuid in self._pull_interfaces.keys():
            try:
                self._remove_socket(uuid)
            except KeyError:
                pass
        self.poller = None
        return "Finished cleanup."

    @cscp_requestable
    def get_data_sources(self, _request: CSCPMessage | None = None) -> Tuple[str, list[str], None]:
        """Get list of connected data sources.

        No payload argument.

        """
        res = []
        num = len(self._pull_interfaces)
        for uuid, host in self._pull_interfaces.items():
            address, port = host
            res.append(f"{address}:{port} ({uuid})")
        return f"{num} connected data sources", res, None

    @chirp_callback(CHIRPServiceIdentifier.DATA)
    def _add_sender_callback(self, service: DiscoveredService) -> None:
        """Callback method for connecting to data service."""
        if not service.alive:
            self._remove_sender(service)
        else:
            self._add_sender(service)

    def _add_sender(self, service: DiscoveredService) -> None:
        """
        Adds an interface (host, port) to receive data from.
        """
        self._pull_interfaces[service.host_uuid] = (service.address, service.port)
        self.log.info("Adding interface tcp://%s:%s to listen to.", service.address, service.port)
        # handle late-coming satellite offers
        if self.fsm.current_state_value in [
            SatelliteState.ORBIT,
            SatelliteState.RUN,
        ]:
            self._add_socket(service.host_uuid, service.address, service.port)

    def _remove_sender(self, service: DiscoveredService) -> None:
        """Removes sender from pool"""
        try:
            self._pull_interfaces.pop(service.host_uuid)
            self._remove_socket(service.host_uuid)
        except KeyError:
            pass

    def _add_socket(self, uuid: UUID, address: str, port: int) -> None:
        interface = f"tcp://{address}:{port}"
        self.log.info("Connecting to %s", interface)
        socket = self.context.socket(zmq.PULL)
        socket.connect(interface)
        self._pull_sockets[uuid] = socket
        assert isinstance(self.poller, zmq.Poller)  # for typing
        self.poller.register(socket, zmq.POLLIN)

    def _remove_socket(self, uuid: UUID) -> None:
        socket = self._pull_sockets.pop(uuid)
        if self.poller:
            self.poller.unregister(socket)
        socket.close()

    def _reset_receiver_stats(self) -> None:
        """Reset internal statistics used for monitoring"""
        self.receiver_stats = {
            "npackets": 0,
            "nbytes": 0,
        }

    def _get_stat(self, stat: str) -> Any:
        """Get a specific metric"""
        return self.receiver_stats[stat]

    def _configure_monitoring(self, interval: float) -> None:
        """Schedule monitoring for internal parameters."""
        self.reset_scheduled_metrics()
        self._reset_receiver_stats()
        for stat in self.receiver_stats:
            self.log.info("Configuring monitoring for '%s' metric", stat)
            # add a callback using partial
            self.schedule_metric(
                stat,
                "",
                MetricsType.LAST_VALUE,
                interval,
                partial(self._get_stat, stat=stat),
            )
