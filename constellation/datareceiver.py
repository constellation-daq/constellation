#!/usr/bin/env python3
"""Base module for Constellation Satellites that receive data."""

import threading
import os
import datetime
import time
from typing import Optional
from queue import Queue, Empty

import h5py
import zmq

from constellation.satellite import Satellite
from constellation.protocol import DataTransmitter
from constellation.datasender import DataBlock


class PullThread(threading.Thread):
    """Thread that pulls DataBlocks from a ZMQ socket and enqueues them."""

    def __init__(
            self, stopevt, interface: int, queue: Queue,
            *args,
            context: Optional[zmq.Context] = None,
            **kwargs,
    ):
        """Initialize values.

        Arguments:
        - stopevt    :: Event that if set lets the thread shut down.
        - port       :: The port to bind to.
        - queue      :: The Queue to process DataBlocks from.
        - context    :: ZMQ context to use (optional).
        """
        super().__init__(*args, **kwargs)
        self.stopevt = stopevt
        self.queue = queue
        ctx = context or zmq.Context()
        self._socket = ctx.socket(zmq.PUSH)
        self._socket.connect(interface)
        self.packet_num = 0

    def run(self):
        """Start receiving data.

        """
        transmitter = DataTransmitter()
        while not self.stopevt.is_set():
            try:
                # non-blocking call to prevent deadlocks
                item = DataBlock(transmitter.recv(self._socket, flags=zmq.NOBLOCK))
                self.queue.put(item)
                self.packet_num += 1
            except zmq.ZMQError:
                # no thing to process, sleep instead
                # TODO consider adjust sleep value
                time.sleep(0.01)
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

    def on_load(self):
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
                stopevt=self._stop_pull_threads,
                interface=f"tcp://{host}:{port}",
                queue=self.data_queue,
                context=self.context
            )
            thread.name = f"{self.name}_{host}_{port}_pull-thread"
            thread.start()
            self._puller_threads.append(thread)
            self.logger.info(f"Satellite {self.name} pulling data from {host}:{port}")

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
                        raise RuntimeError("Could not stop data-pulling thread {t.name} in time.")
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

        This is only an abstract method. Inheriting classes must implement their
        own acquisition method.

        """
        h5file = self._open_file()
        # processing loop
        while not self._stop_running.is_set():
            try:
                # blocking call but with timeout to prevent deadlocks
                item = self.data_queue.get(block=True, timeout=0.5)
                # if we have data, send it
                if isinstance(item, DataBlock):
                    if item.recv_host not in h5file:
                        grp = h5file.create_group(item.recv_host)
                    else:
                        grp = h5file[item.recv_host]
                    evt = grp.create_group(item.meta["eventid"])
                    # TODO add a call to a "write_data" method that can be
                    # overloaded by inheriting classes
                    evt.create_dataset("data", data=item.payload)
                else:
                    raise RuntimeError(f"Unable to handle queue item: {type(item)}")
                self.data_queue.task_done()
            except Empty:
                # nothing to process
                pass
        h5file.close()


# -------------------------------------------------------------------------

def main(args=None):
    """Start the Lecroy oscilloscope device server."""
    import argparse
    import logging

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
    s = H5DataReceiverWriter(args.cmd_port, args.hb_port, args.log_port)
    s.recv_from("localhost", 55557)
    s.run_satellite()


if __name__ == "__main__":
    main()
