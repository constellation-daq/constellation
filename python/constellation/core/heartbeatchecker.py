"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import logging
import threading
import time
from datetime import datetime

import zmq

from typing import Optional, Callable
from .fsm import SatelliteState
from .chp import CHPTransmitter

logger = logging.getLogger("HeartbeatChecker")


class HeartbeatChecker:
    """Checks periodically Satellites' state via subscription to its Heartbeat.

    Individual heartbeat checks run in separate threads. In case of a failure
    (either the Satellite is in ERROR/SAFE state or has missed several
    heartbeats) the corresponding thread will set a `failed` event.
    Alternatively, an action can be triggered via a callback.

    """

    # initial values for period and lives
    HB_INIT_LIVES = 3
    HB_INIT_PERIOD = 2000

    def __init__(self, callback: Optional[Callable] = None) -> None:
        self._callback = callback
        self._callback_lock = threading.Lock()
        self._transmitters = dict[str, CHPTransmitter]()
        self._threads = dict[str, threading.Thread]()
        self._stop_threads: threading.Event = None
        self._states_lock = threading.Lock()
        self.states = dict[str, SatelliteState]()
        self.failed = dict[str, threading.Event]()
        self.auto_recover = False  # clear fail Event if Satellite reappears?

    def register(self, name, host: str, context: Optional[zmq.Context] = None) -> None:
        """Register a heartbeat check for a specific Satellite."""
        ctx = context or zmq.Context()
        socket = ctx.socket(zmq.SUB)
        socket.connect(host)
        socket.setsockopt_string(zmq.SUBSCRIBE, "")
        tm = CHPTransmitter(name, socket)
        evt = threading.Event()
        self._transmitters[name] = tm
        self.failed[name] = evt
        logger.info(f"Registered heartbeating check for {host}")
        return evt

    def _set_state(self, name: str, state: SatelliteState):
        with self._states_lock:
            self.states[name] = state

    def _run_thread(
        self, name: str, transmitter: CHPTransmitter, fail_evt: threading.Event
    ) -> None:
        logger.info(f"Thread for {name} starting heartbeat check")
        lives = self.HB_INIT_LIVES
        interval = self.HB_INIT_PERIOD
        last = datetime.now()
        self._set_state(name, SatelliteState.NEW)
        while not self._stop_threads.is_set():
            last_diff = (datetime.now() - last).total_seconds()
            if last_diff < interval / 1000:
                time.sleep(0.1)
            else:
                host, ts, state, new_interval = transmitter.recv()
                if not state:
                    if last_diff > (interval / 1000) * 1.5:
                        # no message after 150% of the interval, subtract life
                        lives -= 1
                        logger.debug(f"{name} unresponsive, removed life, now {lives}")
                        if lives <= 0:
                            # no lives left, interrupt
                            if not fail_evt.is_set():
                                logger.debug(
                                    f"{name} unresponsive causing interrupt callback to be called"
                                )
                                fail_evt.set()
                                self._interrupt(name)
                            # update states
                            self._set_state(name, SatelliteState.DEAD)
                        # try again later
                        last = datetime.now()
                        continue
                    else:
                        # try again later
                        continue
                # got a heartbeat!
                state = SatelliteState(state)
                interval = new_interval
                lives = self.HB_INIT_LIVES
                # update states
                self._set_state(name, state)
                if state in [
                    SatelliteState.ERROR,
                    SatelliteState.SAFE,
                    SatelliteState.DEAD,
                ]:
                    # other satellite in error state, interrupt
                    if not fail_evt.is_set():
                        logger.debug(
                            f"{name} state causing interrupt callback to be called"
                        )
                        fail_evt.set()
                        self._interrupt(name)
                else:
                    # maybe recover?
                    if self.auto_recover and fail_evt.is_set():
                        fail_evt.clear()
                if ts.to_unix() < last.timestamp():
                    # received hb older than last update; we are lagging behind;
                    # skip to next without updating 'last' (or sleeping)
                    logger.debug(
                        f"{name} lagging behind, consuming heartbeats without rest"
                    )
                    continue
                # update timestamp for this round
                last = datetime.now()

    def _interrupt(self, name: str) -> None:
        with self._callback_lock:
            try:
                self._callback(name)
            except Exception:
                pass

    def _reset(self) -> None:
        self._stop_threads = threading.Event()
        for evt in self.failed.values():
            evt.clear()
        self._threads = dict[str, threading.Thread]()

    def get_failed(self) -> list[str]:
        """Get a list of the names of all failed Satellites."""
        res = list[str]()
        for name, evt in self.failed.items():
            if evt.is_set() or (
                self._stop_threads and not self._threads[name].is_alive()
            ):
                res.append(name)
        return res

    def start(self) -> None:
        """Start the heartbeat checking."""
        # reset lists
        self._reset()
        # set up threads
        for name, tm in self._transmitters.items():
            self._threads[name] = threading.Thread(
                target=self._run_thread,
                args=(name, tm, self.failed[name]),
                daemon=True,  # kill threads when main app closes
            )
        # start threads
        for thread in self._threads.values():
            thread.start()

    def stop(self) -> None:
        """Stop heartbeat checking."""
        if self._stop_threads:
            self._stop_threads.set()
        for thread in self._threads.values():
            thread.join(1)
        for tm in self._transmitters.values():
            tm.close()
        self._reset()


def main():
    """Receive heartbeats from a single host."""
    import argparse
    import time
    import coloredlogs

    parser = argparse.ArgumentParser(description=main.__doc__)
    parser.add_argument("--log-level", default="debug")
    parser.add_argument("--ip", type=str, default="127.0.0.1")
    parser.add_argument("--port", type=int, default=61234)
    args = parser.parse_args()

    # set up logging
    coloredlogs.install(level=args.log_level.upper(), logger=logger)
    logger.info("Starting up heartbeater!")

    def callback(name):
        logger.error(f"Service {name} failed, callback was called!")

    hb_checker = HeartbeatChecker(callback)
    evt = hb_checker.register("some_satellite", f"tcp://{args.ip}:{args.port}")
    hb_checker.start()

    while True:
        failed = ", ".join(hb_checker.get_failed())
        print(
            f"Failed heartbeats so far: {failed}, evt is set: {evt.is_set()}, states: {hb_checker.states}"
        )
        time.sleep(1)

    hb_checker.stop()


if __name__ == "__main__":
    main()
