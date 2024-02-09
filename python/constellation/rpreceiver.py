#!/usr/bin/env python3
"""Base module for Constellation Satellites that receive data."""

import threading
import time
import logging
from queue import Queue

import zmq
from zmq.sugar import Context

from .protocol import DataTransmitter
from .datasender import DataBlock
from .datareceiver import H5DataReceiverWriter, PullThread

""" NOTE: This module only exists to experiment on datareceiver. Hopefully we can rewrite datareceiver with appropriate functions eventually. """


class NewPullThread(PullThread):
    def __init__(
        self,
        stopevt: threading.Event,
        depart: threading.Event,
        interface: str,
        queue: Queue,
        *args,
        context: Context | None = None,
        **kwargs,
    ):
        super().__init__(stopevt, interface, queue, *args, context=context, **kwargs)
        self.depart = depart

    def run(self):
        """Start receiving data."""
        transmitter = DataTransmitter()
        while not self.depart.is_set():
            if not self.stopevt.is_set():
                try:
                    # non-blocking call to prevent deadlocks
                    item = DataBlock(*transmitter.recv(self._socket, flags=zmq.NOBLOCK))

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
                        f"Queue is full. Data {self.packet_num} from {self.item.recv_host} was lost."
                    )
                    continue


class RedPitayaReceiver(H5DataReceiverWriter):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def on_initialize(self):
        return super().on_initialize()

    # NOTE: This method is a bit redundant perhaps. We don't
    def data_callback(self, host: str, port: int, depart: threading.Event):
        self.recv_from(host, port)

        thread = NewPullThread(
            stopevt=self._stop_pulling,
            depart=depart,
            interface=f"tcp://{host}:{port}",
            queue=self.data_queue,
            context=self.context,
            daemon=True,  # terminate with the main thread
        )
        thread.name = f"{self.name}_{host}_{port}_pull-thread"
        self._puller_threads.append(thread)
        self.logger.info(f"Satellite {self.name} pulling data from {host}:{port}")
        thread.run()


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

    # s.recv_from("169.254.45.216", 55557)
    s.recv_from("192.168.1.66", 55557)
    # s.recv_from("localhost", 55557)
    s.run_satellite()


if __name__ == "__main__":
    main()
