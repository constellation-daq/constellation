import time
import threading

import zmq
import msgpack
from typing import Optional, Union, Callable


HB_PERIOD = 1000


class Heartbeater:
    def __init__(self,
                 status_callback: Callable,
                 interface_or_socket: Union[str, zmq.Socket],
                 context: Optional[zmq.Context] = None) -> None:
        if isinstance(interface_or_socket, zmq.Socket):
            self.socket = interface_or_socket
            self.ctx = self.socket.context
        else:
            self.ctx = context or zmq.Context()
            self.socket = self.ctx.socket(zmq.PUB)
            self.socket.bind(interface_or_socket)
        self._stop_heartbeating = threading.Event()
        self._callback = status_callback

    def start(self):
        self._run_thread = threading.Thread(target=self.run, daemon=True)
        self._run_thread.start()

    def stop(self):
        self._stop_heartbeating.set()
        self._run_thread.join()

    def run(self) -> None:
        while not self._stop_heartbeating.is_set():
            state = self._callback()
            dictData = {'time': time.time(), 'state': state}
            self.socket.send(msgpack.packb(dictData), zmq.NOBLOCK)
            time.sleep(HB_PERIOD*1e-3)


def main():
    """Send random heartbeats."""
    import argparse
    parser = argparse.ArgumentParser(description=main.__doc__)
    parser.add_argument("--port", type=int, default=61234)
    parser.add_argument("--num", type=int, default=10, help="Number of heartbeats to send.")
    args = parser.parse_args()

    from fsm import SatelliteState
    from random import randint

    def demo_state():
        return SatelliteState(randint(SatelliteState.IDLE.value, SatelliteState.ERROR.value)).name

    heartbeater = Heartbeater(demo_state, f"tcp://*:{args.port}")
    heartbeater.start()
    ctx = zmq.Context()
    socket = zmq.Socket(ctx, zmq.SUB)
    socket.setsockopt_string(zmq.SUBSCRIBE, '')
    socket.connect(f"tcp://localhost:{args.port}")
    hbs = 0
    while hbs < args.num:
        print(f"Waiting for heartbeat #{hbs}")
        print(SatelliteState[msgpack.unpackb(socket.recv())["state"]])
        hbs += 1
    heartbeater.stop()


if __name__ == '__main__':
    main()
