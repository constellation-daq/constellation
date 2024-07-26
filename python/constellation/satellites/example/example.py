#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides the class for a Constellation Satellite.
"""

from constellation.core.satellite import Satellite, SatelliteArgumentParser
import time
import logging
from typing import Any
from constellation.core.configuration import ConfigError, Configuration
from constellation.core.base import EPILOG


"""
Mock class representing a device that can be utilised by a satellite
"""


class Example_Device1:
    def __init__(self, voltage, ampere, sample_period=0.1):
        self.voltage = voltage
        self.ampere = ampere
        self.sample_period = sample_period


class Example_Satellite(Satellite):

    def do_initializing(self, config: Configuration) -> str:
        try:
            self.device = Example_Device1(
                config["voltage"], config["current"], config["sample_period"]
            )
        except KeyError as e:
            self.log.error(
                "Attribute '%s' is required but missing from the configuration.", e
            )
            raise ConfigError

        return "Initialized"

    def do_run(self, payload: Any) -> str:
        while not self._state_thread_evt.is_set():
            """
            Example work to be done while satellite is running
            """
            time.sleep(self.device.sample_period)
            print(f"New sample at {self.device.voltage} V")
        return "Finished acquisition."


# -------------------------------------------------------------------------


def main(args=None):
    """Start an example satellite.

    Provides a basic example satellite that can be controlled, and used as a basis for implementations.
    """
    import coloredlogs

    parser = SatelliteArgumentParser(description=main.__doc__, epilog=EPILOG)
    # this sets the defaults for our "demo" Satellite
    parser.set_defaults(name="satellite_demo")
    # get a dict of the parsed arguments
    args = vars(parser.parse_args(args))

    # set up logging
    logger = logging.getLogger(args["name"])
    log_level = args.pop("log_level")
    coloredlogs.install(level=log_level.upper(), logger=logger)

    logger.info("Starting up satellite!")
    # start server with remaining args
    s = Example_Satellite(**args)
    s.run_satellite()
