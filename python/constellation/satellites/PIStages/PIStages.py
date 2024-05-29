#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module implements a PI Stage Controller Satellite
"""
from functools import partial

from ..core.satellite import Satellite, SatelliteArgumentParser
from ..core.fsm import SatelliteState
from ..core.base import setup_cli_logging

# PI Python control library
import pipython


class PIStagesSatellite(Satellite):
    """Satellite controlling a PI Stage Controller via the `pipython` library"""

    def do_initializing(self, configuration):
        """Set up connection to stage controller and configure axes."""
        if getattr(self, "device", None):
            # Close stale connection
            self.device.CloseConnection()

        self.device = pipython.GCSDevice(configuration["controller_name"])

        # Setting up the controller
        with self.device as control:
            control.ConnectTCPIP(ipaddress=configuration["controller_ip"])
            self.log.info("Connected to stage controller %s", control.qIDN().strip())
            if control.HasqVER():
                self.log.info(
                    "Controller version info:\n{}".format(control.qVER().strip())
                )

            # Set up the stages:
            self.log.debug("Initializing stages")
            pipython.pitools.startup(
                control,
                stages=configuration["stages"],
                refmodes=configuration["refmodes"],
            )

        # configure metrics sending
        self._configure_monitoring()
        return "Connected to controller and set up stages"

    def do_launching(self, payload):
        """Move the stages to their configured positions."""
        with self.device as control:
            for axis in control.axes:
                # move axis
                # control.MOV(axis, target)

                # Wait for stage to reach target
                pipython.pitools.waitontarget(control, axes=axis)

        return "Moved stage axes to target positions."

    def do_interrupting(self, payload):
        """Stop axes"""
        return "Interrupted and stopped axes."

    def fail_gracefully(self):
        """Stop stages and disconnect."""
        if getattr(self, "device", None):
            # FIXME stop axes, switch off servos
            self.device.CloseConnection()
        return "Stopped axes and disconnected from controller."

    def get_position(self, axis: str):
        """Return the position of the given axis."""
        if self.fsm.current_state in [
            SatelliteState.NEW,
            SatelliteState.ERROR,
            SatelliteState.DEAD,
        ]:
            return None
        try:
            with self.device as control:
                val = control.qPOS(axis)[axis]
        except Exception as e:
            val = None
            self.log.exception(e)
        return val

    def _configure_monitoring(self):
        """Schedule monitoring for certain parameters."""
        self.reset_scheduled_metrics()

        with self.device as control:
            for axis in control.axes:
                self.log.info("Configuring monitoring for axis %s", axis)
                self.schedule_metric(
                    f"position_{axis}",
                    partial(self.get_position, axis=axis),
                    10.0,
                )


# ---


def main(args=None):
    """The PI Stage Motor Controller Satellite for steering PI motor stages"""
    parser = SatelliteArgumentParser(
        description=main.__doc__,
        epilog="This is a 3rd-party component of Constellation.",
    )
    args = vars(parser.parse_args(args))

    # set up logging
    setup_cli_logging(args["name"], args.pop("log_level"))

    # start server with remaining args
    s = PIStagesSatellite(**args)
    s.run_satellite()


if __name__ == "__main__":
    main()
