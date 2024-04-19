"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import time
import threading
from datetime import datetime

import zmq

from .chp import CHPTransmitter
from .fsm import SatelliteStateHandler


class HeartbeatSender(SatelliteStateHandler):
    """Send regular state updates via Constellation Heartbeat Protocol."""

    def __init__(
        self,
        name: str,
        hb_port: int,
        interface: str,
        **kwargs,
    ) -> None:

        super().__init__(name=name, interface=interface, **kwargs)
        self.heartbeat_period = 1000

        # register and start heartbeater
        socket = self.context.socket(zmq.PUB)
        host = f"tcp://{interface}:{hb_port}"
        socket.bind(host)
        self.log.info("Setting up heartbeater socket on %s", host)
        self._hb_tm = CHPTransmitter(self.name, socket)

    def _add_com_thread(self):
        """Add the CHIRP broadcaster thread to the communication thread pool."""
        super()._add_com_thread()
        self._com_thread_pool["heartbeat"] = threading.Thread(
            target=self._run_heartbeat, daemon=True
        )
        self.log.debug("Heartbeat sender thread prepared and added to the pool.")

    def _run_heartbeat(self) -> None:
        last = datetime.now()
        while not self._com_thread_evt.is_set():
            if (
                (datetime.now() - last).total_seconds() > self.heartbeat_period / 1000
            ) or self.fsm.transitioned:
                last = datetime.now()
                state = self.fsm.current_state.value
                self._hb_tm.send(state, int(self.heartbeat_period * 1.1))
                self.fsm.transitioned = False
            else:
                time.sleep(0.1)
        self.log.info("HeartbeatSender thread shutting down.")
        # clean up
        self._hb_tm.close()


def main():
    """Send heartbeats."""
    import argparse
    import logging
    import coloredlogs

    parser = argparse.ArgumentParser(description=main.__doc__)
    parser.add_argument("--log-level", default="info")
    parser.add_argument("--port", type=int, default=61234)
    parser.add_argument("--period", type=int, default=1000)
    parser.add_argument("--interface", type=str, default="127.0.0.1")
    parser.add_argument(
        "--num", type=int, default=10, help="Number of heartbeats to send."
    )
    args = parser.parse_args()

    name = "demo_heartbeater"

    # set up logging
    logger = logging.getLogger(name)
    coloredlogs.install(level=args.log_level.upper(), logger=logger)
    logger.info("Starting up heartbeater!")

    heartbeater = HeartbeatSender(
        name=name, hb_port=args.port, interface=args.interface
    )
    heartbeater.heartbeat_period = args.period
    heartbeater._add_com_thread()
    heartbeater._start_com_threads()
    ctx = zmq.Context()
    socket = zmq.Socket(ctx, zmq.SUB)
    socket.setsockopt_string(zmq.SUBSCRIBE, "")
    socket.connect(f"tcp://{args.interface}:{args.port}")
    hbs = 0
    while args.num < 0 or hbs < args.num:
        print(f"Waiting for heartbeat #{hbs}")
        print(socket.recv())
        hbs += 1


if __name__ == "__main__":
    main()
