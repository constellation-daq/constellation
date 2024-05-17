#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

A base module for a Constellation Satellite that sends data.
"""

import time
import threading
import logging
from typing import Optional
from queue import Queue, Empty

import random
import numpy as np
import zmq

from .cdtp import DataTransmitter, CDTPMessageIdentifier
from .satellite import Satellite, SatelliteArgumentParser
from .base import EPILOG
from .broadcastmanager import CHIRPServiceIdentifier


class PushThread(threading.Thread):
    """Thread that pushes CDTPMessages from a Queue to a ZMQ socket."""

    def __init__(
        self,
        name: str,
        stopevt: threading.Event,
        port: int,
        queue: Queue,
        *args,
        context: Optional[zmq.Context] = None,
        **kwargs,
    ):
        """Initialize values.

        Arguments:
        - name       :: Name of the satellite.
        - stopevt    :: Event that if set lets the thread shut down.
        - port       :: The port to bind to.
        - queue      :: The Queue to process payload and meta of data runs from.
        - context    :: ZMQ context to use (optional).
        """
        super().__init__(*args, **kwargs)
        self.name = name
        self._logger = logging.getLogger(__name__)
        self.stopevt = stopevt
        self.queue = queue
        ctx = context or zmq.Context()
        self._socket = ctx.socket(zmq.PUSH)
        self._socket.bind(f"tcp://*:{port}")

    def run(self):
        """Start sending data."""
        transmitter = DataTransmitter(self.name, self._socket)
        while not self.stopevt.is_set():
            try:
                # blocking call but with timeout to prevent deadlocks
                payload, meta = self.queue.get(block=True, timeout=0.5)
                # if we have data, send it
                if meta == CDTPMessageIdentifier.BOR:
                    transmitter.send_start(payload=payload)
                elif meta == CDTPMessageIdentifier.EOR:
                    transmitter.send_end(payload=payload)
                else:
                    transmitter.send_data(payload=payload, meta=meta)
                self._logger.debug(
                    f"Sending packet number {transmitter.sequence_number}"
                )
                self.queue.task_done()
            except Empty:
                # nothing to process
                pass

    def join(self, *args, **kwargs):
        self._socket.close()
        return super().join(*args, **kwargs)


class DataSender(Satellite):
    """Constellation Satellite which pushes data via ZMQ."""

    def __init__(self, *args, data_port: int, **kwargs):
        super().__init__(*args, **kwargs)

        # set up the data pusher which will transmit
        # data placed into the queue via ZMQ socket
        self.data_queue = Queue()
        self.data_port = data_port
        self.register_offer(CHIRPServiceIdentifier.DATA, data_port)
        self.broadcast_offers()

    def do_launching(self, payload: any) -> str:
        """Launch satellite. Start PushThread."""
        self._stop_pusher = threading.Event()
        self._push_thread = PushThread(
            name=self.name,
            stopevt=self._stop_pusher,
            port=self.data_port,
            queue=self.data_queue,
            context=self.context,
            daemon=True,  # terminate with the main thread
        )
        # self._push_thread.name = f"{self.name}_Pusher-thread"
        self._push_thread.start()
        self.log.info(f"Satellite {self.name} publishing data on port {self.data_port}")
        return super().do_launching(payload)

    def do_landing(self, payload: any) -> str:
        """Land satellite. Stop PushThread."""
        self._stop_pusher.set()
        try:
            self._push_thread.join(timeout=10)
        except TimeoutError:
            self.log.warning("Unable to close push thread. Process timed out.")
        return super().do_landing(payload)

    def _wrap_start(self, payload: any) -> str:
        """Wrapper for the 'run' state of the FSM.

        This method notifies the data queue of the beginning and end of the data run,
        as well as performing basic satellite transitioning.

        """
        self.data_queue.put(("FIXME: Setup of run here?", CDTPMessageIdentifier.BOR))
        ret = super()._wrap_start(payload)
        self.data_queue.put(
            ("FIXME: Info about end of run here?", CDTPMessageIdentifier.EOR)
        )
        return ret

    def do_run(self, payload: any) -> str:
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

    def do_run(self, payload: any) -> str:
        """Example implementation that generates random values."""
        samples = np.linspace(0, 2 * np.pi, 1024, endpoint=False)
        fs = random.uniform(0, 3)
        data_load = np.sin(2 * np.pi * fs * samples)

        t0 = time.time_ns()

        num = 0
        while not self._state_thread_evt.is_set():
            self.data_queue.put((data_load.tobytes(), {"dtype": f"{data_load.dtype}"}))
            self.log.debug(f"Queueing data packet {num}")
            num += 1
            time.sleep(0.5)

        t1 = time.time_ns()
        self.log.info(
            f"total time for {num} evt / {num * len(data_load) / 1024 / 1024}MB: {(t1 - t0)/1000000000}s"
        )
        return "Finished acquisition"


# -------------------------------------------------------------------------


class DataSenderArgumentParser(SatelliteArgumentParser):
    """Customized Argument parser providing DataSender-specific options."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.network.add_argument(
            "--data-port",
            "--cdtp-port",
            type=int,
            help="The port for sending data via the "
            "Constellation Data Transfer Protocol (default: %(default)s).",
        )


def main(args=None):
    """Start the RandomDataSender demonstration satellite.

    This Satellite sends random data via CDTP and can be used to test the
    protocol.

    """
    import coloredlogs

    parser = DataSenderArgumentParser(description=main.__doc__, epilog=EPILOG)
    # this sets the defaults for our "demo" Satellite
    parser.set_defaults(
        name="random_data_sender",
        cmd_port=23998,
        mon_port=55555,
        hb_port=61233,
        data_port=45557,
    )
    args = vars(parser.parse_args(args))

    # set up logging
    logger = logging.getLogger(args["name"])
    log_level = args.pop("log_level")
    coloredlogs.install(level=log_level.upper(), logger=logger)

    # start server with remaining args
    s = RandomDataSender(**args)
    s.run_satellite()


if __name__ == "__main__":
    main()
