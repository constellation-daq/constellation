#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides the class for a Constellation Satellite.
"""

import time
import logging
from constellation.core.base import EPILOG
from constellation.core.commandmanager import cscp_requestable
from constellation.core.configuration import ConfigError, Configuration
from constellation.core.cscp import CSCPMessage
from constellation.core.satellite import Satellite, SatelliteArgumentParser


"""
Mock class representing a device that can be utilised by a satellite
"""


class Example_Device1:
    def __init__(self, voltage, current, sample_period=0.1):
        self.voltage = voltage
        self.current = current
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

    def do_run(self, payload: any) -> str:
        while not self._state_thread_evt.is_set():
            """
            Example work to be done while satellite is running
            """
            time.sleep(self.device.sample_period)
            print(f"New sample at {self.device.voltage} V")
        return "Finished acquisition."

    @cscp_requestable
    def get_current(self, request: CSCPMessage):
        """
        Example custom command. Returns a message containing the current value. Takes unit as argument.
        Allowed in the states ORBIT and RUN.
        """
        paramList = request.payload
        currentUnit = paramList[0]
        return (
            "Device current is " + str(self.device.current) + " " + str(currentUnit),
            None,
            None,
        )

    def _get_current_is_allowed(self, request: CSCPMessage):
        """Controls when custom command is allowed. Allow in the states ORBIT and RUN."""
        return self.fsm.current_state.id in ["ORBIT", "RUN"]


# -------------------------------------------------------------------------


def main(args=None):
    """Start an example satellite.

    Provides a basic example satellite that can be controlled, and used as a basis for implementations.
    """
    import coloredlogs

    parser = SatelliteArgumentParser(description=main.__doc__, epilog=EPILOG)
    # this sets the defaults for our "demo" Satellite
    parser.set_defaults(
        name="satellite_demo", cmd_port=23999, mon_port=55556, hb_port=61234
    )
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


if __name__ == "__main__":
    main()
