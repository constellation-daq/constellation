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
from queue import Empty, Queue, Full
from typing import Optional
import pathlib

import h5py
import numpy as np
import zmq

from .broadcastmanager import chirp_callback, DiscoveredService
from .cdtp import CDTPMessage, CDTPMessageIdentifier, DataTransmitter
from .chirp import CHIRPServiceIdentifier
from .fsm import SatelliteState
from .satellite import Satellite


class PullThread(threading.Thread):
    """Thread that pulls DataBlocks from a ZMQ socket and enqueues them."""

    def __init__(
        self,
        name: str,
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
        self.name = name
        self.stopevt = stopevt
        self.queue = queue
        ctx = context or zmq.Context()
        self._socket = ctx.socket(zmq.PULL)
        self._socket.connect(interface)
        self._logger = logging.getLogger(f"PullThread_port_{interface}")

    def run(self):
        """Start receiving data."""
        transmitter = DataTransmitter(self.name, self._socket)
        while not self.stopevt.is_set():
            try:
                # non-blocking call to prevent deadlocks
                item = transmitter.recv(flags=zmq.NOBLOCK)
                if item:
                    self.queue.put(item)
                    self._logger.debug(
                        f"Received packet as packet number {item.sequence_number}"
                    )
            except zmq.ZMQError:
                # no thing to process, sleep instead
                # TODO consider adjust sleep value
                time.sleep(0.02)
                continue

            # TODO consider case where queue is full
            # NOTE: Due to data_queue being a shared resource it is probably safer to handle the exception
            #       rather than checking
            except Full:
                self._logger.error(
                    f"Queue is full. Data {item.sequence_number} from {item.name} was lost."
                )
                continue

    def join(self, *args, **kwargs):
        self._socket.close()
        return super().join(*args, **kwargs)


def extract_num(key, pattern: re.Pattern, ret=0):
    """Help function used for sorting data keys in write_data_virtual"""
    search = pattern.search(key)
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
        self._puller_threads = dict[str, PullThread]()
        self._stop_pulling = threading.Event()

        self.request(CHIRPServiceIdentifier.DATA)

    def do_initializing(self, payload: any) -> str:
        return super().do_initializing(payload)

    def do_launching(self, payload: any) -> str:
        """Set up threads to listen to interfaces.

        Stops any still-running threads.

        """
        # Set up the data pusher which will transmit
        # data placed into the queue via ZMQ socket.
        self._stop_pulling = threading.Event()
        # TODO self._pull_interfaces should be filled via configuration options
        for uuid, host in self._pull_interfaces.items():
            address, port = host
            self._start_thread(uuid, address, port)

        return super().do_launching(payload)

    def do_landing(self, payload: any) -> str:
        """Stop pull threads."""
        self._stop_pull_threads(10.0)
        return super().do_landing(payload)

    def on_failure(self):
        """Stop all threads."""
        self._stop_pull_threads(2.0)

    def do_run(self, run_number: int) -> str:
        """Handle the data enqueued by the pull threads.

        This method will be executed in a separate thread by the underlying
        Satellite class. It therefore needs to monitor the self.stop_running
        Event and close itself down if the Event is set.

        This is only an abstract method. Inheriting classes must implement their
        own acquisition method.

        """
        raise NotImplementedError

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

        # NOTE: Not sure this is the right way to handle late-coming satellite offers
        if self.fsm.current_state.id in [SatelliteState.ORBIT, SatelliteState.RUN]:
            uuid = str(service.host_uuid)
            self._start_thread(uuid, service.address, service.port)

    def _remove_sender(self, service: DiscoveredService):
        """Removes sender from pool"""
        uuid = str(service.host_uuid)
        self._puller_threads[uuid].join()
        self._pull_interfaces.pop(uuid)
        self._puller_threads.pop(uuid)

    def _start_thread(self, uuid: str, address, port: int):
        thread = PullThread(
            name=uuid,
            stopevt=self._stop_pulling,
            interface=f"tcp://{address}:{port}",
            queue=self.data_queue,
            context=self.context,
            daemon=True,  # terminate with the main thread
        )
        thread.name = f"{self.name}_{address}_{port}_pull-thread"
        thread.start()
        self._puller_threads[uuid] = thread
        self.log.info(f"Satellite {self.name} pulling data from {address}:{port}")

    def _stop_pull_threads(self, timeout=None) -> None:
        """Stop any running threads that pull data."""
        # check that the Event for stopping exists
        if self._stop_pulling:
            # stop any running threads
            self._stop_pulling.set()
            for _, t in self._puller_threads.items():
                if t.is_alive():
                    t.join(timeout)
                    # check in case we timed out:
                    if t.is_alive():
                        raise RuntimeError(
                            "Could not stop data-pulling thread {t.name} in time."
                        )
            self._stop_pulling = None
            self._puller_threads = dict[str, PullThread]()


class H5DataReceiverWriter(DataReceiver):
    """Satellite which receives data via ZMQ writing it to HDF5."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.run_number = 0

        # Tracker for which satellites have joined the current data run.
        self.running_sats = []
        # NOTE: Necessary because of .replace() in _open_file() overwriting the string, thus losing format
        self.file_name_pattern = ""
        self.directroy_name = ""

    def do_initializing(self, payload: any) -> str:
        """Initialize the satellite. Set pattern for file name."""
        self.file_name_pattern = self.config.setdefault(
            "file_name_pattern", "default_name_{run_number}_{date}.h5"
        )
        self.directroy_name = self.config.setdefault("directory_name", "H5_file_dir")
        return "Initializing"

    def _write_data(self, h5file: h5py.File, item: CDTPMessage):
        """Write data into HDF5 format

        Format: h5file -> Group (name) ->   BOR Dataset
                                            Single Concatenated Dataset
                                            EOR Dataset

        Writes data to file by concatenating item.payload to dataset inside group name.
        """
        if item.sequence_number % 100 == 0:
            self.log.debug(
                "Processing data packet %s from %s", item.sequence_number, item.name
            )

        # Check if group already exists.
        if item.msgtype == CDTPMessageIdentifier.BOR and item.name not in h5file.keys():
            self.running_sats.append(item.name)
            try:
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
                    self.run_number,
                )

            except Exception as e:
                self.log.exception(
                    "Failed to create group for dataset. Exception occurred: %s", e
                )

        elif item.msgtype == CDTPMessageIdentifier.DAT:
            try:
                grp = h5file[item.name]
                title = f"data_{self.run_number}_{item.sequence_number}"

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

            except Exception as e:
                self.log.error("Failed to write to file. Exception occurred: %s", e)

        elif item.msgtype == CDTPMessageIdentifier.EOR:
            try:
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
                    self.run_number,
                )
            except Exception as e:
                self.log.error(
                    "Failed to access group for EOR. Exception occurred: %s", e
                )

    def do_run(self, run_number: int) -> str:
        """Handle the data enqueued by the pull threads.

        This method will be executed in a separate thread by the underlying
        Satellite class. It therefore needs to monitor the self.stop_running
        Event and close itself down if the Event is set.

        """

        self.run_number = run_number
        h5file = self._open_file()
        try:
            # processing loop
            while not self._state_thread_evt.is_set() or not self.data_queue.empty():
                try:
                    # blocking call but with timeout to prevent deadlocks
                    item = self.data_queue.get(block=True, timeout=0.5)

                    # if we have data, write it
                    if isinstance(item, CDTPMessage):
                        if (
                            item.msgtype != CDTPMessageIdentifier.BOR
                            and item.name not in self.running_sats
                        ):
                            self.log.warning(
                                f"Received item from {item.name} that is not part of current run"
                            )
                            continue
                        self._write_data(
                            h5file, item
                        )  # NOTE: Could be replaced w/ write_data_concat or write_data_virtual
                    else:
                        raise RuntimeError(f"Unable to handle queue item: {type(item)}")
                    self.data_queue.task_done()

                except Empty:
                    # nothing to process
                    pass
        finally:
            h5file.close()
            self.running_sats = []
            return "Finished Acquisition"

    def _open_file(self):
        """Open the hdf5 file and return the file object."""
        h5file = None
        filename = pathlib.Path(
            self.file_name_pattern.format(
                run_number=self.run_number,
                date=datetime.datetime.now().strftime("%Y-%m-%d-%H%M%S"),
            )
        )
        if os.path.isfile(filename):
            self.log.error("file already exists: %s", filename)
            raise RuntimeError(f"file already exists: {filename}")

        self.log.debug("Creating file %s", filename)
        # Create directory path.
        directory = pathlib.Path(self.directroy_name)  # os.path.dirname(filename)
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


# -------------------------------------------------------------------------


def main(args=None):
    """Start the Constellation data receiver satellite."""
    import argparse
    import coloredlogs

    parser = argparse.ArgumentParser(description=main.__doc__)
    parser.add_argument("--log-level", default="info")
    parser.add_argument("--cmd-port", type=int, default=23989)
    parser.add_argument("--mon-port", type=int, default=55566)
    parser.add_argument("--hb-port", type=int, default=61244)
    parser.add_argument("--interface", type=str, default="*")
    parser.add_argument("--name", type=str, default="h5_data_receiver")
    parser.add_argument("--group", type=str, default="constellation")
    args = parser.parse_args(args)
    # set up logging
    logger = logging.getLogger(args.name)
    coloredlogs.install(level=args.log_level.upper(), logger=logger)
    # start server with remaining args
    s = H5DataReceiverWriter(
        cmd_port=args.cmd_port,
        hb_port=args.hb_port,
        mon_port=args.mon_port,
        name=args.name,
        group=args.group,
        interface=args.interface,
    )

    s.run_satellite()


if __name__ == "__main__":
    main()
