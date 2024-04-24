#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides the class for a Constellation Satellite.
"""

from .satellite import Satellite
import time
import logging
from .configuration import ConfigError, Configuration


class Example_Device1:

    def set_config(self, config: Configuration):
        self.voltage = config["voltage"]
        self.ampere = config["ampere"]
        self.sample_period = config.setdefault("sample_period", default=0.1)


class Example_Satellite(Satellite):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.device = Example_Device1()

    def do_initializing(self, payload: dict) -> str:
        super().do_initializing(payload=payload)
        try:
            self.device.set_config(self.config)
        except KeyError as e:
            self.log.error("Configuration has no attribute %s", e)
            raise ConfigError

        return "Initialized"

    def do_run(self, payload: any) -> str:
        while not self._state_thread_evt.is_set():
            time.sleep(self.device.sample_period)
            print(f"New sample at {self.device.voltage}")
        return "Finished acquisition."


def main(args=None):
    """Start the base Satellite server."""
    import argparse

    parser = argparse.ArgumentParser(description=main.__doc__)
    parser.add_argument("--log-level", default="info")
    parser.add_argument("--cmd-port", type=int, default=23999)
    parser.add_argument("--log-port", type=int, default=5556)
    parser.add_argument("--hb-port", type=int, default=61234)
    parser.add_argument("--name", type=str, default="keithley")
    parser.add_argument("--group", type=str, default="constellation")
    args = parser.parse_args(args)

    # set up logging
    logger = logging.getLogger()  # get root logger
    formatter = logging.Formatter(
        "%(asctime)s | %(name)s |  %(levelname)s: %(message)s"
    )
    # global level should be the lowest level that we want to see on any
    # handler, even streamed via ZMQ
    logger.setLevel(0)

    stream_handler = logging.StreamHandler()
    stream_handler.setLevel(args.log_level.upper())
    stream_handler.setFormatter(formatter)
    logger.addHandler(stream_handler)

    logger.info("Starting up satellite!")
    # start server with remaining args
    s = Example_Satellite(
        args.name,
        args.group,
        args.cmd_port,
        args.hb_port,
        args.log_port,
    )
    s.run_satellite()


if __name__ == "__main__":
    main()
