#!/usr/bin/env python3
"""A base module for a Constellation Satellite that sends data."""

import time
import datetime
import threading
import os
import logging
from typing import Optional
from queue import Queue, Empty

import zmq

from constellation.satellite import Satellite
from constellation.protocol import DataTransmitter


class DataBlock:
    """Class to hold data payload and meta information (map) of an event."""

    def __init__(
        self,
        payload=None,
        meta: dict = None,
        recv_host: str = None,
        recv_ts: datetime.datetime = None,
    ):
        """Initialize variables."""
        self.payload = payload
        self.meta = meta
        self.recv_host = recv_host
        self.recv_ts = recv_ts

    def __str__(self):
        return f"DataBlock (payload: {len(self.payload)}, \
        meta: {len(self.meta)}, recv from host {self.recv_host} \
        at {self.recv_ts})"


class PushThread(threading.Thread):
    """Thread that pushes DataBlocks from a Queue to a ZMQ socket."""

    def __init__(
        self,
        stopevt,
        port: int,
        queue: Queue,
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
        self._logger = logging.getLogger(__name__)
        self.stopevt = stopevt
        self.queue = queue
        self.packet_num = 0
        ctx = context or zmq.Context()
        self._socket = ctx.socket(zmq.PUSH)
        self._socket.bind(f"tcp://*:{port}")

    def run(self):
        """Start sending data."""
        transmitter = DataTransmitter()
        while not self.stopevt.is_set():
            try:
                # blocking call but with timeout to prevent deadlocks
                item = self.queue.get(block=True, timeout=0.5)
                # if we have data, send it
                if isinstance(item, DataBlock):
                    item.meta["packet_num"] = self.packet_num
                    transmitter.send(item.payload, item.meta, self._socket)
                    self._logger.debug(f"Sending packet number {self.packet_num}")
                    self.packet_num += 1
                else:
                    raise RuntimeError(f"Unable to handle queue item: {type(item)}")
                self.queue.task_done()
            except Empty:
                # nothing to process
                pass


class DataSender(Satellite):
    """Constellation Satellite which pushes data via ZMQ."""

    def __init__(self, *args, data_port: int, **kwargs):
        super().__init__(*args, **kwargs)

        # set up the data pusher which will transmit
        # data placed into the queue via ZMQ socket
        self._stop_pusher = threading.Event()
        self.data_queue = Queue()
        self._push_thread = PushThread(
            stopevt=self._stop_pusher,
            port=data_port,
            queue=self.data_queue,
            context=self.context,
            daemon=True,  # terminate with the main thread
        )
        self._push_thread.name = f"{self.name}_Pusher-thread"
        self._push_thread.start()
        self.logger.info(f"Satellite {self.name} publishing data on port {data_port}")

    def do_run(self):
        """Perform the data acquisition and enqueue the results.

        This method will be executed in a separate thread by the underlying
        Satellite class. It therefore needs to monitor the self.stop_running
        Event and close itself down if the Event is set.

        This is only an abstract method. Inheriting classes must implement their
        own acquisition method.

        """
        raise NotImplementedError


class RandomDataSender(DataSender):
    """Constellation Satellite which pushes RANDOM data via ZMQ."""

    def do_run(self):
        """Example implementation that generates random values."""
        payload = os.urandom(1024)

        t0 = time.time()
        num = 0
        while not self._stop_running.is_set():
            meta = {"eventid": num, "time": datetime.datetime.now().isoformat()}
            data = DataBlock(payload, meta)
            self.data_queue.put(data)
            self.logger.debug(f"Queueing data packet {num}")
            num += 1
            time.sleep(0.5)
        t1 = time.time()
        self.logger.info(
            f"total time for {num} evt / {num * len(payload) / 1024 / 1024}MB: {t1 - t0}s"
        )


# -------------------------------------------------------------------------


def main(args=None):
    """Start the Lecroy oscilloscope device server."""
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--log-level", default="info")
    parser.add_argument("--cmd-port", type=int, default=23999)
    parser.add_argument("--log-port", type=int, default=55556)
    parser.add_argument("--hb-port", type=int, default=61234)
    parser.add_argument("--data-port", type=int, default=55557)

    args = parser.parse_args(args)
    logging.basicConfig(
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
        level=args.log_level.upper(),
    )

    # start server with remaining args
    s = RandomDataSender(
        "random_data_sender",
        cmd_port=args.cmd_port,
        hb_port=args.hb_port,
        log_port=args.log_port,
        data_port=args.data_port,
    )
    s.run_satellite()


if __name__ == "__main__":
    main()
