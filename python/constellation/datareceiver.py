#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

Base module for Constellation Satellites that receive data.
"""

import datetime
import logging
import os
import re
import threading
import time
from queue import Empty, Queue
from typing import Optional

import h5py
import numpy as np
import zmq

from .broadcastmanager import CHIRPBroadcaster, DiscoveredService
from .cdtp import CDTPMessage, DataTransmitter
from .chirp import CHIRPServiceIdentifier
from .fsm import SatelliteState
from .satellite import Satellite


class PullThread(threading.Thread):
    """Thread that pulls DataBlocks from a ZMQ socket and enqueues them."""

    def __init__(
        self,
        stopevt: threading.Event,
        interface: str,
        queue: Queue,
        *args,
        context: Optional[zmq.Context] = None,
        **kwargs,
    ):
        """Initialize values.

        Arguments:
        - stopevt    :: Event that if set triggers the thread to shut down.
        - port       :: The port to bind to.
        - queue      :: The Queue to process DataBlocks from.
        - context    :: ZMQ context to use (optional).
        """
        super().__init__(*args, **kwargs)
        self.stopevt = stopevt
        self.queue = queue
        self.packet_num = 0
        ctx = context or zmq.Context()
        self._socket = ctx.socket(zmq.PULL)
        self._socket.connect(interface)
        self._logger = logging.getLogger(f"PullThread_port_{interface}")

    def run(self):
        """Start receiving data."""
        transmitter = DataTransmitter()
        while not self.stopevt.is_set():
            try:
                # non-blocking call to prevent deadlocks
                item = CDTPMessage(*transmitter.recv(self._socket, flags=zmq.NOBLOCK))

                self.queue.put(item)
                self._logger.debug(
                    f"Received packet as packet number {self.packet_num}"
                )
                self.packet_num += 1
            except zmq.ZMQError:
                # no thing to process, sleep instead
                # TODO consider adjust sleep value
                time.sleep(0.02)
                continue

            # TODO consider case where queue is full
            # NOTE: Due to data_queue being a shared resource it is probably safer to handle the exception
            #       rather than checking
            except Queue.full:
                self._logger.error(
                    f"Queue is full. Data {self.packet_num} from {self.item.name} was lost."
                )
                continue


def extract_num(s, p, ret=0):
    """Help function used for sorting data keys in write_data_virtual"""
    search = p.search(s)
    if search:
        return int(search.groups()[0])
    else:
        return ret


class DataReceiver(Satellite):
    """Constellation Satellite which receives data via ZMQ."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.data_queue = Queue()
        self._stop_pulling = None
        self._pull_interfaces = {}
        self._puller_threads = list[PullThread]()
        self._stop_pulling = threading.Event()

        self.register_request(CHIRPServiceIdentifier.DATA, self.recv_from_callback)

    # NOTE: This callback method is not correct. Both BroadcastManager and Satellite needs to be able to handle PullThreads
    def recv_from_callback(
        self, _broadcaster: CHIRPBroadcaster, service: DiscoveredService
    ):
        """
        Callback method for data service.
        Adds an interface (host, port) to receive data from.
        """
        # TODO: Name satellites instead of using host_uuid
        self._pull_interfaces[service.host_uuid] = (service.address, service.port)
        self.log.info(
            f"Adding interface tcp://{service.address}:{service.port} to listen to."
        )

        # NOTE: Not sure this is the right way to handle late-coming satellite offers
        if self.get_state() in [
            SatelliteState.INIT,
            SatelliteState.ORBIT,
            SatelliteState.RUN,
        ]:
            thread = PullThread(
                stopevt=self._stop_pulling,
                interface=f"tcp://{service.address}:{service.port}",
                queue=self.data_queue,
                context=self.context,
                daemon=True,  # terminate with the main thread
            )
            thread.name = f"{self.name}_{service.address}_{service.port}_pull-thread"
            self._puller_threads.append(thread)
            thread.start()
            self.log.info(
                f"Satellite {self.name} pulling data from {service.address}:{service.port}"
            )

    def on_initialize(self):
        """Set up threads to listen to interfaces.

        Stops any still-running threads.

        """
        self._stop_pull_threads(10.0)
        # Set up the data pusher which will transmit
        # data placed into the queue via ZMQ socket.
        self._stop_pulling = threading.Event()
        # TODO self._pull_interfaces should be filled via configuration options
        for _uuid, host in self._pull_interfaces.items():
            address, port = host
            thread = PullThread(
                stopevt=self._stop_pulling,
                interface=f"tcp://{address}:{port}",
                queue=self.data_queue,
                context=self.context,
                daemon=True,  # terminate with the main thread
            )
            thread.name = f"{self.name}_{address}_{port}_pull-thread"
            thread.start()
            self._puller_threads.append(thread)
            self.logger.info(f"Satellite {self.name} pulling data from {host}:{port}")

    def on_failure(self):
        """Stop all threads."""
        self._stop_pull_threads(2.0)

    def do_run(self):
        """Handle the data enqueued by the pull threads.

        This method will be executed in a separate thread by the underlying
        Satellite class. It therefore needs to monitor the self.stop_running
        Event and close itself down if the Event is set.

        This is only an abstract method. Inheriting classes must implement their
        own acquisition method.

        """
        raise NotImplementedError

    def _stop_pull_threads(self, timeout=None) -> None:
        """Stop any running threads that pull data."""
        # check that the Event for stopping exists
        if self._stop_pulling:
            # stop any running threads
            self._stop_pulling.set()
            for t in self._puller_threads:
                if t.is_alive():
                    t.join(timeout)
                    # check in case we timed out:
                    if t.is_alive():
                        raise RuntimeError(
                            "Could not stop data-pulling thread {t.name} in time."
                        )
            self._stop_pulling = None
            self._puller_threads = list[PullThread]()


class H5DataReceiverWriter(DataReceiver):
    """Satellite which receives data via ZMQ writing it to HDF5."""

    def __init__(self, *args, filename: str, **kwargs):
        super().__init__(*args, **kwargs)

        self.filename = filename
        # NOTE: Necessary because of .replace() in _open_file() overwriting the string, thus losing format
        self.nameformat = filename

    def do_initializing(self, payload: any) -> str:
        super().do_initializing(payload)
        self.nameformat = self.config.setdefault(
            "name_format", "default_name_{date}.h5"
        )
        return "Initializing"

    def _open_file(self):
        """Open the hdf5 file and return the file object."""
        h5file = None
        self.filename = self.nameformat.format(
            date=datetime.datetime.now().strftime("%Y-%m-%d-%H%M%S")
        )
        print(self.filename)  # TODO: remove after debugging
        if os.path.isfile(self.filename):
            self.log.error(f"file already exists: {self.filename}")
            raise RuntimeError(f"file already exists: {self.filename}")

        self.log.debug("Creating file %s", self.filename)
        # Create directory path.
        directory = os.path.dirname(self.filename)
        try:
            os.makedirs(directory)
        except (FileExistsError, FileNotFoundError):
            pass
        except Exception as exception:
            raise RuntimeError(
                f"unable to create directory {directory}: \
                {type(exception)} {str(exception)}"
            ) from exception
        try:
            h5file = h5py.File(self.filename, "w")
        except Exception as exception:
            self.log.error("Unable to open %s: %s", self.filename, str(exception))
            raise RuntimeError(
                f"Unable to open {self.filename}: {str(exception)}",
            ) from exception
        return h5file

    def write_data(self, h5file, item):
        """Write data to HDF5 format

        Current format: File -> Grp (recv_host) -> Dataset (event_{eventid)

        Writes data to file by adding new dataset to group recv_host.

        # TODO add a call to a "write_data" method that can be
        # overloaded by inheriting classes
        """
        # Check if group already exists
        if item.name not in h5file.keys():
            grp = h5file.create_group(item.name)
        else:
            grp = h5file[item.name]

        # Create dataset and write data (item) to it
        print(f"event_{item.meta['eventid']}")  # TODO: remove this
        dset = grp.create_dataset(
            f"event_{item.meta['eventid']}",
            data=np.frombuffer(item.payload, dtype=item.meta["dtype"]),
            chunks=True,
            dtype=item.meta["dtype"],
        )
        dset.attrs["CLASS"] = "DETECTOR_DATA"
        for key, val in item.meta.items():
            dset.attrs[key] = val
        self.log.debug(f"Processing data packet {item.meta['packet_num']}")

    def write_data_concat(self, h5file: h5py.File, item: CDTPMessage):
        """Write data into HDF5 format

        Format: h5file -> Group (name) -> Single Concatenated Dataset (item)

        Writes data to file by concatenating item.payload to dataset inside group name.

        # TODO add a call to a "write_data" method that can be
        # overloaded by inheriting classes
        """
        # Check if group already exists
        if item.name not in h5file.keys():
            grp = h5file.create_group(item.name)

            dset = grp.create_dataset(
                "data",
                data=np.frombuffer(item.payload, dtype=item.meta["dtype"]),
                chunks=True,
                dtype=item.meta["dtype"],
                maxshape=(None,),
            )

            dset.attrs["CLASS"] = "DETECTOR_DATA"
            for key, val in item.meta.items():
                dset.attrs[key] = val

        else:
            # Extend current dataset with data obtained from item
            grp = h5file[item.name]
            new_data = np.frombuffer(item.payload, dtype=item.meta["dtype"])
            grp["data"].resize((grp["data"].shape[0] + new_data.shape[0]), axis=0)
            grp["data"][-new_data.shape[0] :] = new_data

        self.log.debug(f"Processing data packet {item.meta['packet_num']}")

    def write_data_virtual(self, h5file: h5py.File, item: CDTPMessage):
        """Write data to HDF5 format

        Format: h5file -> Group (name) -> Multiple Datasets (item) + Virtual Dataset

        Writes data by adding a dataset containing item.payload to group name. Also builds
        a virtual dataset from the group.

        # TODO add a call to a "write_data" method that can be
        # overloaded by inheriting classes
        """
        # Check if group already exists
        if item.name not in h5file.keys():
            grp = h5file.create_group(item.name)
        else:
            grp = h5file[item.name]

        dset = grp.create_dataset(
            f"event_{item.meta['eventid']}",
            data=np.frombuffer(item.payload, dtype=item.meta["dtype"]),
            chunks=True,
            dtype=item.meta["dtype"],
        )
        dset.attrs["CLASS"] = "DETECTOR_DATA"
        for key, val in item.meta.items():
            dset.attrs[key] = val
        self.log.debug(f"Processing data packet {item.meta['packet_num']}")

        # Create a virtual layout with the datasets in group
        # TODO: this method will result in index-variable overflow, make it so only the latest X datasets are shown
        #       or split into multiple virtual datasets at a certain point
        layout = h5py.VirtualLayout(
            shape=(50000,), dtype="i4"  # TODO: Replace 50000 w/ something appropriate
        )
        last_index = 0
        p = re.compile(r"(\d+)")
        for data_name in sorted(list(grp.keys()), key=lambda s: extract_num(s, p)):
            if data_name != "vdata":
                data_file = h5file[item.name][data_name]
                vsource = h5py.VirtualSource(data_file)
                layout[last_index : last_index + len(data_file)] = vsource
                last_index += len(data_file) + 1
        if "vdata" in grp.keys():
            del h5file[item.name]["vdata"]
        grp.create_virtual_dataset("vdata", layout, fillvalue=-1)

    def do_run(self):
        """Handle the data enqueued by the pull threads.

        This method will be executed in a separate thread by the underlying
        Satellite class. It therefore needs to monitor the self.stop_running
        Event and close itself down if the Event is set.

        """

        h5file = self._open_file()
        try:
            # processing loop
            while not self._stop_running.is_set() or not self.data_queue.empty():
                try:
                    # blocking call but with timeout to prevent deadlocks
                    item = self.data_queue.get(block=True, timeout=0.5)

                    self.log.debug(f"Received: {item}")

                    # if we have data, send it
                    if isinstance(item, CDTPMessage):
                        self.write_data_concat(
                            h5file, item
                        )  # Could be replaced w/ write_data_concat or write_data_virtual
                    else:
                        raise RuntimeError(f"Unable to handle queue item: {type(item)}")
                    self.data_queue.task_done()

                except Empty:
                    # nothing to process
                    pass
        finally:
            h5file.close()


# -------------------------------------------------------------------------


def main(args=None):
    """Start the Constellation data receiver satellite."""
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--log-level", default="info")
    parser.add_argument("--cmd-port", type=int, default=23989)
    parser.add_argument("--log-port", type=int, default=55566)
    parser.add_argument("--hb-port", type=int, default=61244)

    args = parser.parse_args(args)
    logging.basicConfig(
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
        level=args.log_level.upper(),
    )

    # start server with remaining args
    s = H5DataReceiverWriter(
        "h5_data_receiver",
        cmd_port=args.cmd_port,
        hb_port=args.hb_port,
        log_port=args.log_port,
    )

    s.run_satellite()


if __name__ == "__main__":
    main()
