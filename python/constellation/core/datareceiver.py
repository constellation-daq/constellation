#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

Base module for Constellation Satellites that receive data.
"""

import datetime
import logging
import os
import pathlib

import h5py
import numpy as np
import zmq

from . import __version__
from .broadcastmanager import chirp_callback, DiscoveredService
from .cdtp import CDTPMessage, CDTPMessageIdentifier, DataTransmitter
from .chirp import CHIRPServiceIdentifier
from .commandmanager import cscp_requestable
from .cscp import CSCPMessage
from .fsm import SatelliteState
from .satellite import Satellite, SatelliteArgumentParser
from .base import EPILOG


class DataReceiver(Satellite):
    """Constellation Satellite which receives data via ZMQ."""

    def __init__(self, *args, **kwargs):
        # define our attributes
        self._pull_interfaces = {}
        self._pull_sockets = {}
        self.poller = None
        # initialize Satellite attributes
        super().__init__(*args, **kwargs)
        self.request(CHIRPServiceIdentifier.DATA)

    def do_initializing(self, payload: any) -> str:
        return super().do_initializing(payload)

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
        """Handle the data enqueued by the pull threads.

        This method will be executed in a separate thread by the underlying
        Satellite class. It therefore needs to monitor the self.stop_running
        Event and close itself down if the Event is set.

        This is only an abstract method. Inheriting classes must implement their
        own acquisition method.

        """
        raise NotImplementedError

    def fail_gracefully(self):
        """Method called when reaching 'ERROR' state."""
        for uuid in self._pull_interfaces.keys():
            try:
                self._remove_socket(uuid)
            except KeyError:
                pass
        self.poller = None

    @cscp_requestable
    def get_data_sources(self, _request: CSCPMessage = None) -> (str, None, None):
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
        # TODO: Name satellites instead of using host_uuid
        self._pull_interfaces[service.host_uuid] = (service.address, service.port)
        self.log.info(
            f"Adding interface tcp://{service.address}:{service.port} to listen to."
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
        self.log.info(f"Connecting to {interface}")
        socket = self.context.socket(zmq.PULL)
        socket.connect(interface)
        self._pull_sockets[uuid] = socket
        self.poller.register(socket, zmq.POLLIN)

    def _remove_socket(self, uuid):
        socket = self._pull_sockets.pop(uuid)
        if self.poller:
            self.poller.unregister(socket)
        socket.close()


class H5DataReceiverWriter(DataReceiver):
    """Satellite which receives data via ZMQ writing it to HDF5."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.run_identifier = ""
        # Tracker for which satellites have joined the current data run.
        self.running_sats = []

    def do_initializing(self, payload: any) -> str:
        """Initialize and configure the satellite."""
        # what pattern to use for the file names?
        self.file_name_pattern = self.config.setdefault(
            "file_name_pattern", "run_{run_identifier}_{date}.h5"
        )
        # what directory to store files in?
        self.output_path = self.config.setdefault("output_path", "data")
        # how often will the file be flushed? Negative values for 'at the end of the run'
        self.flush_interval = self.config.setdefault("flush_interval", 10.0)
        return "Configured all values"

    def _write_data(self, h5file: h5py.File, item: CDTPMessage):
        """Write data into HDF5 format

        Format: h5file -> Group (name) ->   BOR Dataset
                                            Single Concatenated Dataset
                                            EOR Dataset

        Writes data to file by concatenating item.payload to dataset inside group name.
        """
        # Check if group already exists.
        if item.msgtype == CDTPMessageIdentifier.BOR and item.name not in h5file.keys():
            self.running_sats.append(item.name)
            grp = h5file.create_group(item.name).create_group("BOR")
            # add meta information as attributes
            grp.update(item.meta)

            if item.payload:
                dset = grp.create_dataset(
                    "payload",
                    data=item.payload,
                    dtype=item.meta.get("dtype", None),
                )
            self.log.info(
                "Wrote BOR packet from %s on run %s",
                item.name,
                self.run_identifier,
            )

        elif item.msgtype == CDTPMessageIdentifier.DAT:
            try:
                grp = h5file[item.name]
            except KeyError:
                # late joiners
                self.log.error(f"{item.name} sent data without BOR.")
                self.running_sats.append(item.name)
                grp = h5file.create_group(item.name)

            title = f"data_{self.run_identifier}_{item.sequence_number}"

            # interpret bytes as array of uint8 if nothing else was specified in the meta
            payload = np.frombuffer(
                item.payload, dtype=item.meta.get("dtype", np.uint8)
            )

            dset = grp.create_dataset(
                title,
                data=payload,
                chunks=True,
            )

            dset.attrs["CLASS"] = "DETECTOR_DATA"
            dset.attrs.update(item.meta)

        elif item.msgtype == CDTPMessageIdentifier.EOR:
            grp = h5file[item.name].create_group("EOR")
            # add meta information as attributes
            grp.update(item.meta)

            if item.payload:
                dset = grp.create_dataset(
                    "payload",
                    data=item.payload,
                    dtype=item.meta.get("dtype", None),
                )

            self.log.info(
                "Wrote EOR packet from %s on run %s",
                item.name,
                self.run_identifier,
            )

    def do_run(self, run_identifier: str) -> str:
        """Handle the data enqueued by the pull threads.

        This method will be executed in a separate thread by the underlying
        Satellite class. It therefore needs to monitor the self.stop_running
        Event and close itself down if the Event is set.

        """

        self.run_identifier = run_identifier
        h5file = self._open_file()
        self._add_version(h5file)
        last_flush = datetime.datetime.now()
        last_msg = datetime.datetime.now()
        # keep the data collection alive for a few seconds after stopping
        keep_alive = datetime.datetime.now()
        transmitter = DataTransmitter(None, None)
        try:
            # processing loop
            while (
                not self._state_thread_evt.is_set()
                and (datetime.datetime.now() - keep_alive).total_seconds() < 4
            ):
                # refresh keep_alive timestamp
                if not self._state_thread_evt.is_set():
                    keep_alive = datetime.datetime.now()
                # request available data from zmq poller; timeout prevents
                # deadlock when stopping.
                sockets_ready = dict(self.poller.poll(timeout=250))

                for socket in sockets_ready.keys():
                    binmsg = socket.recv_multipart()
                    item = transmitter.decode(binmsg)
                    self._write_data(h5file, item)
                    if (datetime.datetime.now() - last_msg).total_seconds() > 1.0:
                        if self._state_thread_evt.is_set():
                            msg = "Finishing with"
                        else:
                            msg = "Processing"
                        self.log.debug(
                            "%s data packet %s from %s",
                            msg,
                            item.sequence_number,
                            item.name,
                        )
                        last_msg = datetime.datetime.now()

                # time to flush data to file?
                if (
                    self.flush_interval > 0
                    and (datetime.datetime.now() - last_flush).total_seconds()
                    > self.flush_interval
                ):
                    h5file.flush()
                    last_flush = datetime.datetime.now()
        finally:
            h5file.close()
            self.running_sats = []
        return "Finished Acquisition"

    def _open_file(self) -> h5py.File:
        """Open the hdf5 file and return the file object."""
        h5file = None
        filename = pathlib.Path(
            self.file_name_pattern.format(
                run_identifier=self.run_identifier,
                date=datetime.datetime.now().strftime("%Y-%m-%d-%H%M%S"),
            )
        )
        if os.path.isfile(filename):
            self.log.error("file already exists: %s", filename)
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
            self.log.error("Unable to open %s: %s", filename, str(exception))
            raise RuntimeError(
                f"Unable to open {filename}: {str(exception)}",
            ) from exception
        return h5file

    def _add_version(self, h5file: h5py.File):
        """Add version information to file."""
        grp = h5file.create_group(self.name)
        grp["constellation_version"] = __version__


# -------------------------------------------------------------------------


def main(args=None):
    """Start the Constellation data receiver satellite.

    Data will be written in HDF5 format.

    """
    import coloredlogs

    parser = SatelliteArgumentParser(description=main.__doc__, epilog=EPILOG)
    # this sets the defaults for our "demo" Satellite
    parser.set_defaults(
        name="h5_data_receiver", cmd_port=23989, mon_port=55566, hb_port=61244
    )
    # get a dict of the parsed arguments
    args = vars(parser.parse_args(args))

    # set up logging
    logger = logging.getLogger(args["name"])
    log_level = args.pop("log_level")
    coloredlogs.install(level=log_level.upper(), logger=logger)

    # start server with remaining args
    s = H5DataReceiverWriter(**args)
    s.run_satellite()


if __name__ == "__main__":
    main()
