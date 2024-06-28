"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import logging
import threading
import time
from datetime import datetime

import zmq

from typing import Optional, Callable, Any
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

    def __init__(
        self, callback: Optional[Callable[[str, SatelliteState], None]] = None
    ) -> None:
        self._callback = callback
        self._callback_lock = threading.Lock()
        self._transmitters = dict[str, CHPTransmitter]()
        self._threads = dict[str, threading.Thread]()
        self._stop_threads: threading.Event | None = None
        self._states_lock = threading.Lock()
        self._states = dict[str, SatelliteState]()
        self._failed = dict[str, threading.Event]()
        self.auto_recover = False  # clear fail Event if Satellite reappears?

    def register(
        self, name: str, address: str, context: Optional[zmq.Context] = None  # type: ignore[type-arg]
    ) -> threading.Event:
        """Register a heartbeat check for a specific Satellite.

        Returns threading.Event that will be set when a failure occurs.

        """
        ctx = context or zmq.Context()
        socket = ctx.socket(zmq.SUB)
        socket.connect(address)
        socket.setsockopt_string(zmq.SUBSCRIBE, "")
        tm = CHPTransmitter(name, socket)
        evt = threading.Event()
        self._transmitters[name] = tm
        self._failed[name] = evt
        logger.info(f"Registered heartbeating check for {address}")
        return evt

    def is_registered(self, name: str) -> bool:
        """Check whether a given Satellite is already registered."""
        return name in self._transmitters.keys()

    @property
    def states(self) -> dict[str, SatelliteState]:
        """Return a dictionary of the monitored Satellites' state."""
        return self._states

    @property
    def fail_events(self) -> dict[str, threading.Event]:
        """Return a dictionary of Events triggered for failed Satellites."""
        return self._failed

    def _set_state(self, name: str, state: SatelliteState) -> None:
        with self._states_lock:
            self._states[name] = state

    def _run_thread(
        self, name: str, transmitter: CHPTransmitter, fail_evt: threading.Event
    ) -> None:
        logger.info(f"Thread for {name} starting heartbeat check")
        lives = self.HB_INIT_LIVES
        interval = self.HB_INIT_PERIOD
        last = datetime.now()
        self._set_state(name, SatelliteState.NEW)
        # assert for mypy static type analysis
        assert isinstance(
            self._stop_threads, threading.Event
        ), "Thread Event not set up correctly"
        while not self._stop_threads.is_set():
            last_diff = (datetime.now() - last).total_seconds()
            if last_diff < interval / 1000:
                time.sleep(0.1)
            else:
                host, ts, stateid, new_interval = transmitter.recv()
                if not stateid or not ts or not new_interval:
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
                                self._interrupt(name, SatelliteState.DEAD)
                            # update states
                            self._set_state(name, SatelliteState.DEAD)
                        # try again later
                        last = datetime.now()
                        continue
                    else:
                        # try again later
                        continue
                # got a heartbeat!
                state = SatelliteState(stateid)
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
                        self._interrupt(name, state)
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

    def _interrupt(self, name: str, state: SatelliteState) -> None:
        with self._callback_lock:
            if self._callback:
                try:
                    self._callback(name, state)
                except Exception:
                    pass

    def _reset(self) -> None:
        self._stop_threads = threading.Event()
        for evt in self._failed.values():
            evt.clear()
        self._threads = dict[str, threading.Thread]()

    def get_failed(self) -> list[str]:
        """Get a list of the names of all failed Satellites."""
        res = list[str]()
        for name, evt in self._failed.items():
            if evt.is_set() or (
                self._stop_threads and not self._threads[name].is_alive()
            ):
                res.append(name)
        return res

    def start(self, name: str) -> None:
        """Start the heartbeat checking for a single registered Satellite."""
        # prepare
        if not self._stop_threads:
            self._stop_threads = threading.Event()
        self._failed[name].clear()
        # set up thread
        self._threads[name] = threading.Thread(
            target=self._run_thread,
            args=(name, self._transmitters[name], self._failed[name]),
            daemon=True,  # kill threads when main app closes
        )
        # start thread
        self._threads[name].start()

    def start_all(self) -> None:
        """Start the heartbeat checking for all registered Satellites."""
        # reset lists
        self._reset()
        # set up threads
        for name, tm in self._transmitters.items():
            self._threads[name] = threading.Thread(
                target=self._run_thread,
                args=(name, tm, self._failed[name]),
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


def main(args: Any = None) -> None:
    """Receive heartbeats from a single host."""
    import argparse
    import time
    import coloredlogs  # type: ignore[import-untyped]

    parser = argparse.ArgumentParser(description=main.__doc__)
    parser.add_argument("--log-level", default="debug")
    parser.add_argument("--ip", type=str, default="127.0.0.1")
    parser.add_argument("--port", type=int, default=61234)
    args = parser.parse_args()

    # set up logging
    coloredlogs.install(level=args.log_level.upper(), logger=logger)
    logger.info("Starting up heartbeater!")

    def callback(name: str, _state: SatelliteState) -> None:
        logger.error(f"Service {name} failed, callback was called!")

    hb_checker = HeartbeatChecker(callback)
    evt = hb_checker.register("some_satellite", f"tcp://{args.ip}:{args.port}")
    hb_checker.start_all()

    while True:
        failed = ", ".join(hb_checker.get_failed())
        print(
            f"Failed heartbeats so far: {failed}, evt is set: {evt.is_set()}, states: {hb_checker._states}"
        )
        time.sleep(1)

    hb_checker.stop()


if __name__ == "__main__":
    main()
