"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import logging
import threading
import time
from datetime import datetime, timezone

import zmq

from typing import Optional, Callable, Any, cast
from .fsm import SatelliteState
from .chp import CHPDecodeMessage
from .base import ConstellationLogger

logging.setLoggerClass(ConstellationLogger)
logger = cast(ConstellationLogger, logging.getLogger(__name__))


class HeartbeatState:
    def __init__(self, name: str, evt: threading.Event, lives: int, interval: int):
        self.name = name
        self.lives = lives
        self.interval = interval
        self.last_refresh = datetime.now(timezone.utc)
        self.state = SatelliteState.NEW
        self.failed: threading.Event = evt

    def refresh(self, ts: datetime | None = None) -> None:
        if not ts:
            self.last_refresh = datetime.now(timezone.utc)
        else:
            self.last_refresh = ts

    @property
    def seconds_since_refresh(self) -> float:
        return (datetime.now(timezone.utc) - self.last_refresh).total_seconds()


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
        self._threads: threading.Thread | None = None
        self._stop_threads: threading.Event | None = None
        self._poller = zmq.Poller()
        # dict to keep states mapped to socket
        self._states = dict[zmq.Socket, HeartbeatState]()  # type: ignore[type-arg]
        self._socket_lock = threading.Lock()
        self.auto_recover = False  # clear fail Event if Satellite reappears?

    def register(
        self, name: str, address: str, context: Optional[zmq.Context] = None  # type: ignore[type-arg]
    ) -> threading.Event:
        """Register a heartbeat check for a specific Satellite.

        Returns threading.Event that will be set when a failure occurs.

        """
        ctx = context or zmq.Context()
        try:
            socket = ctx.socket(zmq.SUB)
        except zmq.ZMQError as e:
            if "Too many open files" in e.strerror:
                logger.error(
                    "System reports too many open files: cannot open further connections.\n"
                    "Please consider increasing the limit of your OS."
                    "On Linux systems, use 'ulimit' to set a higher value."
                )
            raise e
        socket.connect(address)
        socket.setsockopt_string(zmq.SUBSCRIBE, "")
        evt = threading.Event()
        self._states[socket] = HeartbeatState(
            name, evt, self.HB_INIT_LIVES, self.HB_INIT_PERIOD
        )
        logger.info(f"Registered heartbeating check for {address}")
        return evt

    def unregister(self, name: str) -> None:
        """Unregister a heartbeat check for a specific Satellite."""
        s: zmq.Socket | None = None  # type: ignore[type-arg]
        for socket, hb in self._states.items():
            if hb.name == name:
                s = socket
                break
        if not s:
            return
        with self._socket_lock:
            self._poller.unregister(s)
            self._states.pop(s)
            s.close()
        logger.info("Removed heartbeat check for %s", name)

    def is_registered(self, name: str) -> bool:
        """Check whether a given Satellite is already registered."""
        registered = False
        for hb in self._states.values():
            if hb.name == name:
                registered = True
                break
        return registered

    @property
    def states(self) -> dict[str, SatelliteState]:
        """Return a dictionary of the monitored Satellites' state."""
        res = {}
        for hb in self._states.values():
            res[hb.name] = hb.state
        return res

    @property
    def fail_events(self) -> dict[str, threading.Event]:
        """Return a dictionary of Events triggered for failed Satellites."""
        res = {}
        for hb in self._states.values():
            if hb.failed.is_set():
                res[hb.name] = hb.failed
        return res

    def _run_thread(self) -> None:
        logger.info("Starting heartbeat check thread")
        last_check = datetime.now(timezone.utc)

        # refresh all tokens
        for hb in self._states.values():
            hb.refresh()

        # assert for mypy static type analysis
        assert isinstance(
            self._stop_threads, threading.Event
        ), "Thread Event not set up correctly"

        while not self._stop_threads.is_set():
            # check for heartbeats ready to be received
            with self._socket_lock:
                sockets_ready = dict(self._poller.poll(timeout=50))
                for socket in sockets_ready.keys():
                    binmsg = socket.recv()
                    _host, timestamp, state, interval = CHPDecodeMessage(binmsg)
                    hb = self._states[socket]
                    # update values
                    hb.refresh(timestamp.to_datetime())
                    hb.state = SatelliteState(state)
                    hb.interval = interval
                    # refresh lives
                    if hb.lives != self.HB_INIT_LIVES:
                        logger.log(
                            5,
                            "%s had %d lives left (interval %d), refreshing",
                            hb.name,
                            hb.lives,
                            hb.interval,
                        )
                    hb.lives = self.HB_INIT_LIVES
                    if hb.state in [
                        SatelliteState.ERROR,
                        SatelliteState.SAFE,
                        SatelliteState.DEAD,
                    ]:
                        # satellite in error state, interrupt
                        if not hb.failed.is_set():
                            logger.debug(
                                f"{hb.name} state causing interrupt callback to be called"
                            )
                            hb.failed.set()
                            self._interrupt(hb.name, hb.state)
                    else:
                        # recover?
                        if self.auto_recover and hb.failed.is_set():
                            hb.failed.clear()

            # regularly check for stale connections and missed heartbeats
            if (datetime.now(timezone.utc) - last_check).total_seconds() > 0.3:
                for hb in self._states.values():
                    if hb.seconds_since_refresh > (hb.interval / 1000) * 1.5:
                        # no message after 150% of the interval, subtract life
                        hb.lives -= 1
                        logger.log(
                            5,
                            "%s unresponsive, removed life, now %d",
                            hb.name,
                            hb.lives,
                        )
                        if hb.lives <= 0:
                            # no lives left, interrupt
                            if not hb.failed.is_set():
                                logger.info(
                                    f"{hb.name} unresponsive causing interrupt callback to be called"
                                )
                                hb.failed.set()
                                self._interrupt(hb.name, SatelliteState.DEAD)
                                # update state
                                hb.state = SatelliteState.DEAD
                            # try again later
                            hb.refresh()
                        else:
                            # refresh, try again later
                            hb.refresh()
                # update timestamp for this round
                last_check = datetime.now(timezone.utc)
            # finally, wait a moment
            time.sleep(0.05)

    def _interrupt(self, name: str, state: SatelliteState) -> None:
        with self._callback_lock:
            if self._callback:
                try:
                    self._callback(name, state)
                except Exception:
                    pass

    def get_failed(self) -> list[str]:
        """Get a list of the names of all failed Satellites."""
        res = list[str]()
        for hb in self._states.values():
            if hb.failed.is_set():
                res.append(hb.name)
        return res

    def _run(self) -> None:
        """Run background thread performing the heartbeat checking."""
        # prepare
        if not self._stop_threads:
            self._stop_threads = threading.Event()
            # set up thread
            self._threads = threading.Thread(
                target=self._run_thread,
                daemon=True,  # kill threads when main app closes
            )
            # start thread
            self._threads.start()

    def start(self, name: str) -> None:
        """Start the heartbeat checking for a single Satellite."""
        registered = False
        for socket, hb in self._states.items():
            if hb.name == name:
                with self._socket_lock:
                    self._poller.register(socket, zmq.POLLIN)
                registered = True
                break
        if not registered:
            logger.error(f"Could not find registered Satellite with name '{name}'")
            raise RuntimeError(
                f"Could not find registered Satellite with name '{name}'"
            )
        self._run()

    def start_all(self) -> None:
        """Start the heartbeat checking all registered Satellites."""
        with self._socket_lock:
            for socket in self._states.keys():
                self._poller.register(socket, zmq.POLLIN)
        self._run()

    def stop(self) -> None:
        """Stop heartbeat checking."""
        if self._stop_threads:
            self._stop_threads.set()
            if self._threads:
                self._threads.join(2)
        for socket in self._states.keys():
            self._poller.unregister(socket)
        self._stop_threads = None
        self._threads = None

    def close(self) -> None:
        for socket in self._states.keys():
            socket.close()
        self._states = dict[zmq.Socket, HeartbeatState]()  # type: ignore[type-arg]


def main(args: Any = None) -> None:
    """Receive heartbeats from a single host."""
    import argparse
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
