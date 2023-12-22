import logging
import threading

import zmq
import msgpack

from typing import Optional, Callable
from fsm import SatelliteState

HB_INIT_LIFES = 3
HB_PERIOD = 1000


class HeartbeatChecker:
    """Checks periodically Satellites' state via subscription to its Heartbeat.

    Individual heartbeat checks run in separate threads. In case of a failure
    (either the Satellite is in ERROR/SAFE state or has missed several
    heartbeats) the corresponding thread will set a `failed` event.
    Alternativly, an action can be triggered via a callback.

    """

    def __init__(self, callback: Optional[Callable] = None) -> None:
        self._callback = callback
        self._callback_lock = threading.Lock()
        self._sockets = list[zmq.Socket]()
        self._threads = list[threading.Thread]()
        self._stop_threads = threading.Event()
        self.names = list[str]()
        self.failed = list[threading.Event]()

    def register(self,
                 name,
                 interface: str,
                 context: Optional[zmq.Context] = None) -> None:
        """Register a heartbeat check for a specific Satellite."""
        ctx = context or zmq.Context()
        socket = ctx.socket(zmq.SUB)
        socket.connect(interface)
        socket.setsockopt_string(zmq.SUBSCRIBE, '')
        socket.setsockopt(zmq.RCVTIMEO, int(1.5 * HB_PERIOD))
        self._sockets.append(socket)
        self.names.append(name)
        logging.info(f'Registered heartbeating check for {interface}')

    def _run_thread(self, name: str,
                    socket: zmq.Socket,
                    fail_evt: threading.Event) -> None:
        logging.info(f'Thread {name} starting heartbeat check')
        lifes = HB_INIT_LIFES
        while not self._stop_threads.is_set():
            try:
                message_bin = socket.recv()
                message = msgpack.unpackb(message_bin)
                state = SatelliteState[message['state']]
                logging.debug(f'Thread {name} got state {state}')
                if state == SatelliteState.ERROR or state == SatelliteState.SAFE:
                    # other satellite in error state, interrupt
                    fail_evt.set()
                    self._interrupt(name)
                    break
                lifes = HB_INIT_LIFES
            except zmq.error.Again:
                # no message after 1.5s, substract life
                lifes -= 1
                logging.debug(f'Thread {name} removed life, now {lifes}')
                if lifes <= 0:
                    # no lifes left, interrupt
                    fail_evt.set()
                    self._interrupt(name)
                    break

    def _interrupt(self, name: str) -> None:
        with self._callback_lock:
            try:
                self._callback()
            except Exception:
                pass

    def _reset(self) -> None:
        self._stop_threads = threading.Event()
        self.failed = list[threading.Event]()
        self._threads = list[threading.Thread]()

    def get_failed(self) -> list[str]:
        """Get a list of the names of all failed Satellites."""
        res = list[str]()
        for idx, name in enumerate(self.names):
            if self.failed[idx].is_set():
                res.append(name)
        return res

    def start(self) -> None:
        """Start the heartbeat checking."""
        # reset lists
        self._reset()
        # set up threads
        for idx, socket in enumerate(self._sockets):
            self.failed.append(threading.Event())
            self._threads.append(
                threading.Thread(
                    target=self._run_thread,
                    args=(self.names[idx], socket, self.failed[idx]),
                    daemon=True  # kill threads when main app closes
                ))
        # start threads
        for thread in self._threads:
            thread.start()

    def stop(self) -> None:
        """Stop heartbeat checking."""
        self._stop_threads.set()
        for thread in self._threads:
            thread.join()
        self._reset()


if __name__ == '__main__':
    import argparse
    import time

    parser = argparse.ArgumentParser()
    parser.add_argument("--log-level", default='debug')
    parser.add_argument("--ip", type=str, default='127.0.0.1')
    parser.add_argument("--port", type=int, default=61234)
    parser.add_argument("--timeout", type=int, default=10)
    args = parser.parse_args()

    logging.basicConfig(
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
        level=args.log_level.upper(),
    )

    def callback():
        print("Callback was called!")

    hb_checker = HeartbeatChecker(callback)
    hb_checker.register_check("some_satellite", f"tcp://{args.ip}:{args.port}")
    hb_checker.start()

    timeout = args.timeout
    while timeout > 0:
        failed = ", ".join(hb_checker.get_failed())
        print(f"Failed heartbeats so far: {failed}")
        time.sleep(1)
        timeout -= 1

    hb_checker.stop()
