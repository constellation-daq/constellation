#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

Base module for Constellation Satellites that receive data.
"""

import datetime
import os
import pathlib
import sys

import h5py
import numpy as np
import zmq
from functools import partial

from . import __version__
from .broadcastmanager import chirp_callback, DiscoveredService
from .cdtp import CDTPMessage, CDTPMessageIdentifier, DataTransmitter
from .chirp import CHIRPServiceIdentifier
from .commandmanager import cscp_requestable
from .cscp import CSCPMessage
from .fsm import SatelliteState
from .satellite import Satellite, SatelliteArgumentParser
from .base import EPILOG, setup_cli_logging
from .error import debug_log, handle_error


class DataReceiver(Satellite):
    """Constellation Satellite which receives data via ZMQ."""

    def __init__(self, *args, **kwargs):
        # define our attributes
        self._pull_interfaces = {}
        self._pull_sockets = {}
        self.poller = None
        self.run_identifier = ""
        # Tracker for which satellites have joined the current data run.
        self.active_satellites = []
        # metrics
        self.receiver_stats = None
        # initialize Satellite attributes
        super().__init__(*args, **kwargs)
        self.request(CHIRPServiceIdentifier.DATA)

    def do_initializing(self, config: dict[str]) -> str:
        """Initialize and configure the satellite."""
        # what pattern to use for the file names?
        self.file_name_pattern = self.config.setdefault(
            "file_name_pattern", "run_{run_identifier}_{date}.h5"
        )
        # what directory to store files in?
        self.output_path = self.config.setdefault("output_path", "data")
        self._configure_monitoring(2.0)
        return "Configured DataReceiver"

    def do_launching(self, payload: any) -> str:
        """Set up pull sockets to listen to incoming data."""
        # Set up the data poller which will monitor all ZMQ sockets
        self.poller = zmq.Poller()
        # TODO implement a filter based on configuration values
        for uuid, host in self._pull_interfaces.items():
            address, port = host
            self._add_socket(uuid, address, port)
        return "Established connections to data senders."

    def do_landing(self, payload: any) -> str:
        """Close all open sockets."""
        for uuid in self._pull_interfaces.keys():
            self._remove_socket(uuid)
        self.poller = None
        return "Closed connections to data senders."

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
        transmitter = DataTransmitter(None, None)
        self._reset_receiver_stats()
        try:
            # processing loop
            while not self._state_thread_evt.is_set() or (
                keep_alive
                and (datetime.datetime.now() - keep_alive).total_seconds() < 60
            ):
                # refresh keep_alive timestamp
                if not self._state_thread_evt.is_set():
                    keep_alive = datetime.datetime.now()
                else:
                    if not self.active_satellites:
                        # no Satellites connected
                        keep_alive = None
                        self.log.info("All EORE received, stopping.")
                # request available data from zmq poller; timeout prevents
                # deadlock when stopping.
                sockets_ready = dict(self.poller.poll(timeout=250))

                for socket in sockets_ready.keys():
                    binmsg = socket.recv_multipart()
                    # NOTE below we determine the size of the list of (binary)
                    # strings, which is not exactly what went over the network
                    self.receiver_stats["nbytes"] += sys.getsizeof(binmsg)
                    self.receiver_stats["npackets"] += 1
                    item = transmitter.decode(binmsg)
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
                        self.log.critical(
                            "Could not write message '%s' to file: %s", item, repr(e)
                        )
                        raise RuntimeError(
                            f"Could not write message '{item}' to file"
                        ) from e
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
                    "Never received EORE from following Satellites: %s",
                    ", ".join(self.active_satellites),
                )
            self.active_satellites = []
        return f"Finished acquisition to {filename}"

    def _write_data(self, outfile: any, item: CDTPMessage):
        """Write data to file"""
        raise NotImplementedError()

    def _write_EOR(self, outfile: any, item: CDTPMessage):
        """Write EOR to file"""
        raise NotImplementedError()

    def _write_BOR(self, outfile: any, item: CDTPMessage):
        """Write BOR to file"""
        raise NotImplementedError()

    def _open_file(self, filename: str) -> any:
        """Return the filehandler"""
        raise NotImplementedError()

    def _close_file(self, outfile: any) -> None:
        """Close the filehandler"""
        raise NotImplementedError()

    def do_stopping(self, payload: any):
        """Unused.

        In this Satellite class, this method is not used. All stopping actions
        need to be performed from within `do_run`.

        """
        pass

    def fail_gracefully(self):
        """Method called when reaching 'ERROR' state."""
        for uuid in self._pull_interfaces.keys():
            try:
                self._remove_socket(uuid)
            except KeyError:
                pass
        self.poller = None

    @cscp_requestable
    def get_data_sources(self, _request: CSCPMessage = None) -> (str, list[str], None):
        """Get list of connected data sources.

        No payload argument.

        """
        res = []
        num = len(self._pull_interfaces)
        for uuid, host in self._pull_interfaces.items():
            address, port = host
            res.append(f"{address}:{port} ({uuid})")
        return f"{num} connected data sources", res, None

    @handle_error
    @debug_log
    def _wrap_stop(self, payload: any) -> str:
        """Wrapper for the 'stopping' transitional state of the FSM.

        As the DataReceiver will have to keep files open, we design the `do_run`
        to handle all actions necessary for stopping a run.

        """
        # indicate to the current acquisition thread to stop
        if self._state_thread_evt:
            self._state_thread_evt.set()
        # wait for result, waiting until done
        self.log.info("Waiting for RUN thread to finish.")
        self._state_thread_fut.result(timeout=None)
        self.log.info("RUN thread finished.")
        # NOTE: no call to `do_stopping`
        return "Acquisition stopped"

    @chirp_callback(CHIRPServiceIdentifier.DATA)
    def _add_sender_callback(self, service: DiscoveredService):
        """Callback method for connecting to data service."""
        if not service.alive:
            self._remove_sender(service)
        else:
            self._add_sender(service)

    def _add_sender(self, service: DiscoveredService):
        """
        Adds an interface (host, port) to receive data from.
        """
        self._pull_interfaces[service.host_uuid] = (service.address, service.port)
        self.log.info(
            "Adding interface tcp://%s:%s to listen to.", service.address, service.port
        )
        # handle late-coming satellite offers
        if self.fsm.current_state.id in [SatelliteState.ORBIT, SatelliteState.RUN]:
            uuid = str(service.host_uuid)
            self._add_socket(uuid, service.address, service.port)

    def _remove_sender(self, service: DiscoveredService):
        """Removes sender from pool"""
        uuid = str(service.host_uuid)
        self._pull_interfaces.pop(uuid)
        try:
            self._remove_socket(uuid)
        except KeyError:
            pass

    def _add_socket(self, uuid, address, port):
        interface = f"tcp://{address}:{port}"
        self.log.info("Connecting to %s", interface)
        socket = self.context.socket(zmq.PULL)
        socket.connect(interface)
        self._pull_sockets[uuid] = socket
        self.poller.register(socket, zmq.POLLIN)

    def _remove_socket(self, uuid):
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

    def _get_stat(self, stat: str) -> any:
        """Get a specific metric"""
        return self.receiver_stats[stat]

    def _configure_monitoring(self, interval: float):
        """Schedule monitoring for internal parameters."""
        self.reset_scheduled_metrics()
        self._reset_receiver_stats()
        for stat in self.receiver_stats:
            self.log.info("Configuring monitoring for '%s' metric", stat)
            # add a callback using partial
            self.schedule_metric(
                stat,
                partial(self._get_stat, stat=stat),
                interval,
            )


class H5DataReceiverWriter(DataReceiver):
    """Satellite which receives data via ZMQ and writes to HDF5."""

    def do_initializing(self, config: dict[str]) -> str:
        """Initialize and configure the satellite."""
        super().do_initializing(config)
        # how often will the file be flushed? Negative values for 'at the end of
        # the run'
        self.flush_interval = self.config.setdefault("flush_interval", 10.0)
        return "Configured all values"

    def do_run(self, run_identifier: str) -> str:
        """Handle the data enqueued by the ZMQ Poller."""
        self.last_flush = datetime.datetime.now()
        return super().do_run(run_identifier)

    def _write_EOR(self, outfile: h5py.File, item: CDTPMessage):
        """Write data to file"""
        grp = outfile[item.name].create_group("EOR")
        # add meta information as attributes
        grp.update(item.payload)
        self.log.info(
            "Wrote EOR packet from %s on run %s",
            item.name,
            self.run_identifier,
        )

    def _write_BOR(self, outfile: h5py.File, item: CDTPMessage):
        """Write BOR to file"""
        if item.name not in outfile.keys():
            grp = outfile.create_group(item.name).create_group("BOR")
            # add payload dict information as attributes
            grp.update(item.payload)
            self.log.info(
                "Wrote BOR packet from %s on run %s",
                item.name,
                self.run_identifier,
            )

    def _write_data(self, outfile: h5py.File, item: CDTPMessage):
        """Write data into HDF5 format

        Format: h5file -> Group (name) ->   BOR Dataset
                                            Single Concatenated Dataset
                                            EOR Dataset

        Writes data to file by concatenating item.payload to dataset inside group name.
        """
        # Check if group already exists.
        try:
            grp = outfile[item.name]
        except KeyError:
            # late joiners
            self.log.warning("%s sent data without BOR.", item.name)
            self.active_satellites.append(item.name)
            grp = outfile.create_group(item.name)

        title = f"data_{self.run_identifier}_{item.sequence_number}"

        # interpret bytes as array of uint8 if nothing else was specified in the meta
        payload = np.frombuffer(item.payload, dtype=item.meta.get("dtype", np.uint8))

        dset = grp.create_dataset(
            title,
            data=payload,
            chunks=True,
        )

        dset.attrs["CLASS"] = "DETECTOR_DATA"
        dset.attrs.update(item.meta)

        # time to flush data to file?
        if (
            self.flush_interval > 0
            and (datetime.datetime.now() - self.last_flush).total_seconds()
            > self.flush_interval
        ):
            outfile.flush()
            self.last_flush = datetime.datetime.now()

    def _open_file(self, filename: pathlib.Path) -> h5py.File:
        """Open the hdf5 file and return the file object."""
        h5file = None
        if os.path.isfile(filename):
            self.log.critical("file already exists: %s", filename)
            raise RuntimeError(f"file already exists: {filename}")

        self.log.info("Creating file %s", filename)
        # Create directory path.
        directory = pathlib.Path(self.output_path)  # os.path.dirname(filename)
        try:
            os.makedirs(directory)
        except (FileExistsError, FileNotFoundError):
            self.log.info("Directory %s already exists", directory)
            pass
        except Exception as exception:
            raise RuntimeError(
                f"unable to create directory {directory}: \
                {type(exception)} {str(exception)}"
            ) from exception
        try:
            h5file = h5py.File(directory / filename, "w")
        except Exception as exception:
            self.log.critical("Unable to open %s: %s", filename, str(exception))
            raise RuntimeError(
                f"Unable to open {filename}: {str(exception)}",
            ) from exception
        self._add_version(h5file)
        return h5file

    def _close_file(self, outfile: h5py.File) -> None:
        """Close the filehandler"""
        outfile.close()

    def _add_version(self, outfile: h5py.File):
        """Add version information to file."""
        grp = outfile.create_group(self.name)
        grp["constellation_version"] = __version__


# -------------------------------------------------------------------------


def main(args=None):
    """Start the Constellation data receiver satellite.

    Data will be written in HDF5 format.

    """
    parser = SatelliteArgumentParser(description=main.__doc__, epilog=EPILOG)
    # this sets the defaults for our "demo" Satellite
    parser.set_defaults(
        name="h5_data_receiver", cmd_port=23989, mon_port=55566, hb_port=61244
    )
    # get a dict of the parsed arguments
    args = vars(parser.parse_args(args))

    # set up logging
    setup_cli_logging(args["name"], args.pop("log_level"))

    # start server with remaining args
    s = H5DataReceiverWriter(**args)
    s.run_satellite()


if __name__ == "__main__":
    main()
