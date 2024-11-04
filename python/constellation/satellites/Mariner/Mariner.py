#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

Provides the class for the Mariner example satellite
"""

import time
import random
from typing import Any

from constellation.core.satellite import Satellite, SatelliteArgumentParser
from constellation.core.configuration import Configuration
from constellation.core.fsm import SatelliteState

from constellation.core.base import setup_cli_logging
from constellation.core.base import EPILOG

from constellation.core.cmdp import MetricsType
from constellation.core.cscp import CSCPMessage
from constellation.core.commandmanager import cscp_requestable
from constellation.core.monitoring import schedule_metric


class CanopusStarTracker:
    """Example for a star sensing and tracking device class.

    This class only mocks the features of a real tracker, of course, but don't
    let that spoil your immersion ;)

    """

    def __init__(self, voltage, ampere, sample_period=0.1):
        self.voltage = voltage
        self.ampere = ampere
        self.sample_period = sample_period
        self.locked = True
        self.attitude = 0

    def get_current_brightness(self) -> int:
        """Determine brightness of current field of view.

        Will be static when locked to Canopus."""
        if self.locked:
            return random.randint(94, 96)
        return random.randint(0, 100)

    def canopus_in_view(self) -> bool:
        """Determine whether Canopus is in view."""
        return self.get_current_brightness() > 90

    def get_attitude(self) -> int:
        """Determine and return the current roll attitude."""
        if not self.locked:
            # have not yet found Canopus; search and lock
            while not self.canopus_in_view():
                self.attitude += 1
            self.locked = True
        return self.attitude


class Mariner(Satellite):
    """Example for a Satellite class."""

    def do_initializing(self, config: Configuration) -> str:
        """Configure the Satellite and any associated hardware.

        The configuration is provided as Configuration object which works
        similar to a regular dictionary but tracks access to its keys. If a key
        does not exist or if one exists but is not used, the Satellite will
        automatically return an error or warning, respectively.

        """
        self.device = CanopusStarTracker(config["voltage"], config["current"], config["sample_period"])
        return "Initialized"

    def do_run(self, payload: Any) -> str:
        """The main run routine.

        Here, the main part of the mission would be performed.

        """
        while not self._state_thread_evt.is_set():
            # Example work to be done while satellite is running
            ...
            time.sleep(self.device.sample_period)
            print(f"New sample at {self.device.voltage} V")
        return "Finished acquisition."

    @cscp_requestable
    def get_attitude(self, request: CSCPMessage) -> tuple[str, Any, dict]:
        """Determine and return the space craft's attitude.

        This is an example for a command that can be triggered from a Controller
        via CSCP. The return value of the function consists of a message, a
        payload value and an (optional) dictionary with meta information.

        """
        # we cannot perform this command when not ready:
        if self.fsm.current_state_value in [
            SatelliteState.NEW,
            SatelliteState.ERROR,
            SatelliteState.DEAD,
            SatelliteState.initializing,
            SatelliteState.reconfiguring,
        ]:
            return "Canopus Star Tracker not ready", None, {}
        return "Canopus Star Tracker locked and ready", self.device.get_attitude(), {}

    @schedule_metric("lm", MetricsType.LAST_VALUE, 10)
    def brightness(self) -> int | None:
        if self.fsm.current_state_value in [
            SatelliteState.NEW,
            SatelliteState.ERROR,
            SatelliteState.DEAD,
            SatelliteState.initializing,
            SatelliteState.reconfiguring,
        ]:
            return None
        return self.device.get_current_brightness()


# -------------------------------------------------------------------------


def main(args=None):
    """Start an example satellite.

    Provides a basic example satellite that can be controlled, and used as a
    basis for custom implementations.

    """
    parser = SatelliteArgumentParser(description=main.__doc__, epilog=EPILOG)

    # get a dict of the parsed arguments
    args = vars(parser.parse_args(args))

    # set up logging
    setup_cli_logging(args["name"], args.pop("log_level"))

    # start server with remaining args
    s = Mariner(**args)
    s.run_satellite()
