#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

A base module for a Constellation Satellite that sends data.
"""

import time
import threading
import logging
from typing import Any
from queue import Queue, Empty

import random
import numpy as np
import zmq

from .cdtp import DataTransmitter, CDTPMessageIdentifier
from .satellite import Satellite, SatelliteArgumentParser
from .base import EPILOG, setup_cli_logging
from .broadcastmanager import CHIRPServiceIdentifier


class PushThread(threading.Thread):
    """Thread that pushes CDTPMessages from a Queue to a ZMQ socket."""

    def __init__(
        self,
        name: str,
        stopevt: threading.Event,
        socket: zmq.Socket,  # type: ignore[type-arg]
        queue: Queue,  # type: ignore[type-arg]
        *args: Any,
        **kwargs: Any,
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
        self._socket = socket

    def run(self) -> None:
        """Start sending data."""
        transmitter = DataTransmitter(self.name, self._socket)
        while not self.stopevt.is_set():
            try:
                # blocking call but with timeout to prevent deadlocks
                payload, meta = self.queue.get(block=True, timeout=0.5)
                # if we have data, send it
                if meta == CDTPMessageIdentifier.BOR:
                    transmitter.send_start(payload=payload["payload"], meta=payload["meta"])
                elif meta == CDTPMessageIdentifier.EOR:
                    transmitter.send_end(payload=payload["payload"], meta=payload["meta"])
                else:
                    transmitter.send_data(payload=payload, meta=meta)
                self._logger.debug(f"Sending packet number {transmitter.sequence_number}")
                self.queue.task_done()
            except Empty:
                # nothing to process
                pass

    def join(self, *args: Any, **kwargs: Any) -> Any:
        return super().join(*args, **kwargs)


class DataSender(Satellite):
    """Constellation Satellite which pushes data via ZMQ."""

    def __init__(self, *args: Any, data_port: int, **kwargs: Any):
        # initialize local attributes first:
        # beginning and end-of-run events: payloads and meta information
        self._beg_of_run: dict[str, dict[str, Any]] = {"payload": {}, "meta": {}}
        self._end_of_run: dict[str, dict[str, Any]] = {"payload": {}, "meta": {}}
        # set up the data pusher which will transmit data placed into the queue
        # via ZMQ socket
        self.data_queue: Queue = Queue()  # type: ignore[type-arg]
        self.data_port = data_port

        # initialize satellite
        super().__init__(*args, **kwargs)

        ctx = self.context or zmq.Context()
        self.socket = ctx.socket(zmq.PUSH)

        if not self.data_port:
            self.data_port = self.socket.bind_to_random_port(f"tcp://{self.interface}")
        else:
            self.socket.bind(f"tcp://{self.interface}:{self.data_port}")

        # run CHIRP
        self.register_offer(CHIRPServiceIdentifier.DATA, self.data_port)
        self.broadcast_offers()

    def reentry(self) -> None:
        # close the socket
        self.socket.close()
        self.log.debug("Closed data socket")
        super().reentry()

    @property
    def EOR(self) -> Any:
        """Get optional playload for the end-of-run event (EOR)."""
        return self._end_of_run["payload"]

    @EOR.setter
    def EOR(self, payload: Any) -> None:
        """Set optional playload for the end-of-run event (EOR)."""
        self._end_of_run["payload"] = payload

    @property
    def BOR(self) -> Any:
        """Get optional playload for the beginning-of-run event (BOR)."""
        return self._beg_of_run["payload"]

    @BOR.setter
    def BOR(self, payload: Any) -> None:
        """Set optional playload for the beginning-of-run event (BOR)."""
        self._beg_of_run["payload"] = payload

    def _wrap_launch(self, payload: Any) -> str:
        """Wrapper for the 'launching' transitional state of the FSM.

        This method starts the PushThread for the DataSender.

        """
        self._stop_pusher = threading.Event()
        self._push_thread = PushThread(
            name=self.name,
            stopevt=self._stop_pusher,
            socket=self.socket,
            queue=self.data_queue,
            daemon=True,  # terminate with the main thread
        )
        # self._push_thread.name = f"{self.name}_Pusher-thread"
        self._push_thread.start()
        self.log.info(f"Satellite {self.name} publishing data on port {self.data_port}")
        res: str = super()._wrap_launch(payload)
        return res

    def _wrap_land(self, payload: Any) -> str:
        """Wrapper for the 'landing' transitional state of the FSM.

        This method will stop the PushThread.

        """
        self._stop_pusher.set()
        try:
            self._push_thread.join(timeout=10)
        except TimeoutError:
            self.log.warning("Unable to close push thread. Process timed out.")
        res: str = super()._wrap_land(payload)
        return res

    def _wrap_start(self, run_identifier: str) -> str:
        """Wrapper for the 'run' state of the FSM.

        This method notifies the data queue of the beginning and end of the data run,
        as well as performing basic satellite transitioning.

        """
        # Beginning of run event. If nothing was provided by the user, use the
        # configuration dictionary as a payload
        if not self.BOR:
            self.BOR = self.config._config
        self.log.debug("Sending BOR")
        self.data_queue.put((self._beg_of_run, CDTPMessageIdentifier.BOR))
        res: str = super()._wrap_start(run_identifier)
        return res

    def _wrap_stop(self, payload: Any) -> str:
        """Wrapper for the 'stopping' transitional state of the FSM.

        Sends the EOR event after base class wrapper and `do_stopping` have
        finished.

        """
        res: str = super()._wrap_stop(payload)
        self.log.debug("Sending EOR")
        self.data_queue.put((self._end_of_run, CDTPMessageIdentifier.EOR))
        return res

    def do_run(self, payload: Any) -> str:
        """Perform the data acquisition and enqueue the results.

        This is only an abstract method. Inheriting classes must implement their
        own acquisition method.

        This method will be executed in a separate thread by the underlying
        Satellite class. It therefore needs to monitor the self.stop_running
        Event and close itself down if the Event is set.

        If you want to transmit a payload as part of the end-of-run event (BOR),
        set the corresponding value via the `BOR` property before leaving this
        method.

        This method should return a string that will be used for setting the
        Status once the data acquisition is finished.

        """
        raise NotImplementedError


class RandomDataSender(DataSender):
    """Constellation Satellite which pushes RANDOM data via ZMQ."""

    def do_run(self, payload: Any) -> str:
        """Example implementation that generates random values."""
        samples = np.linspace(0, 2 * np.pi, 1024, endpoint=False)
        fs = random.uniform(0, 3)
        data_load = np.sin(2 * np.pi * fs * samples)

        t0 = time.time_ns()

        num = 0
        # assert for mypy static type analysis
        assert isinstance(self._state_thread_evt, threading.Event)

        while not self._state_thread_evt.is_set():
            self.data_queue.put((data_load.tobytes(), {"dtype": f"{data_load.dtype}"}))
            self.log.debug(f"Queueing data packet {num}")
            num += 1
            time.sleep(0.5)

        t1 = time.time_ns()
        self.log.info(f"total time for {num} evt / {num * len(data_load) / 1024 / 1024}MB: {(t1 - t0) / 1000000000}s")
        return "Finished acquisition"


# -------------------------------------------------------------------------


class DataSenderArgumentParser(SatelliteArgumentParser):
    """Customized Argument parser providing DataSender-specific options."""

    def __init__(self, *args: Any, **kwargs: Any):
        super().__init__(*args, **kwargs)
        self.network.add_argument(
            "--data-port",
            "--cdtp-port",
            type=int,
            help="The port for sending data via the " "Constellation Data Transfer Protocol (default: %(default)s).",
        )


def main(args: Any = None) -> None:
    """Start the RandomDataSender demonstration satellite.

    This Satellite sends random data via CDTP and can be used to test the
    protocol.

    """
    parser = DataSenderArgumentParser(description=main.__doc__, epilog=EPILOG)
    args = vars(parser.parse_args(args))

    # set up logging
    setup_cli_logging(args["name"], args.pop("log_level"))

    # start server with remaining args
    s = RandomDataSender(**args)
    s.run_satellite()


if __name__ == "__main__":
    main()
