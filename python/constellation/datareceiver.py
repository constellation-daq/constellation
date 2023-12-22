#!/usr/bin/env python3
"""Base module for Constellation Satellites that receive data."""

import threading
import os
import datetime
import time
import logging
from typing import Optional
from queue import Queue, Empty

import h5py
import numpy as np
import zmq

from .satellite import Satellite
from .protocol import DataTransmitter
from .datasender import DataBlock


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
                item = DataBlock(*transmitter.recv(self._socket, flags=zmq.NOBLOCK))
                # TODO consider case where queue is full
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


class DataReceiver(Satellite):
    """Constellation Satellite which receives data via ZMQ."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.data_queue = Queue()
        self._stop_pulling = None
        self._pull_interfaces = {}
        self._puller_threads = list[PullThread]()

    def recv_from(self, host: str, port: int) -> None:
        """Adds an interface (host, port) to receive data from."""
        self._pull_interfaces[host] = port
        self.logger.info(f"Adding interface tcp://{host}:{port} to listen to.")

    def on_initialize(self):
        """Set up threads to listen to interfaces.

        Stops any still-running threads.

        """
        self._stop_pull_threads(10.0)
        # Set up the data pusher which will transmit
        # data placed into the queue via ZMQ socket.
        self._stop_pulling = threading.Event()
        # TODO self._pull_interfaces should be filled via configuration options
        for host, port in self._pull_interfaces.items():
            thread = PullThread(
                stopevt=self._stop_pulling,
                interface=f"tcp://{host}:{port}",
                queue=self.data_queue,
                context=self.context,
                daemon=True,  # terminate with the main thread
            )
            thread.name = f"{self.name}_{host}_{port}_pull-thread"
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
        """Stop any runnning threads that pull data."""
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

    def _open_file(self):
        """Open the hdf5 file and return the file object."""
        h5file = None
        self.filename = self.filename.replace(
            "{date}", datetime.datetime.now().strftime("%Y-%m-%d-%H%M%S")
        )

        if os.path.isfile(self.filename):
            self.logger.error(f"file already exists: {self.filename}")
            raise RuntimeError(f"file already exists: {self.filename}")

        self.logger.debug("Creating file %s", self.filename)
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
            self.logger.error("Unable to open %s: %s", self.filename, str(exception))
            raise RuntimeError(
                f"Unable to open {self.filename}: {str(exception)}",
            ) from exception
        return h5file

    def do_run(self):
        """Handle the data enqueued by the pull threads.

        This method will be executed in a separate thread by the underlying
        Satellite class. It therefore needs to monitor the self.stop_running
        Event and close itself down if the Event is set.

        """
        h5file = self._open_file()
        # processing loop
        while not self._stop_running.is_set() or not self.data_queue.empty():
            try:
                # blocking call but with timeout to prevent deadlocks
                item = self.data_queue.get(block=True, timeout=0.5)
                self.logger.debug(f"Received: {item}")
                # if we have data, send it
                if isinstance(item, DataBlock):
                    if item.recv_host not in h5file.keys():
                        grp = h5file.create_group(item.recv_host)
                    else:
                        grp = h5file[item.recv_host]
                    evt = grp.create_group(f"event_{item.meta['eventid']}")
                    # TODO add a call to a "write_data" method that can be
                    # overloaded by inheriting classes
                    dset = evt.create_dataset(
                        "data",
                        data=np.frombuffer(item.payload, dtype=np.uint8),
                        chunks=True,
                        dtype="uint8",
                    )
                    dset.attrs["CLASS"] = "DETECTOR_DATA"
                    for key, val in item.meta.items():
                        dset.attr[key] = val
                    self.logger.debug(
                        f"Processing data packet {item.meta['packet_num']}"
                    )
                else:
                    raise RuntimeError(f"Unable to handle queue item: {type(item)}")
                self.data_queue.task_done()
            except Empty:
                # nothing to process
                pass
        # TODO the file should be closed in case of an exception
        h5file.close()


# -------------------------------------------------------------------------


def main(args=None):
    """Start the Lecroy oscilloscope device server."""
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
        filename="test_data_{date}.h5",
    )

    s.recv_from("localhost", 55557)
    s.run_satellite()


if __name__ == "__main__":
    main()
