"""This module provides a Constellation Satellite for controlling a Keithley power supply."""

import yaml

from constellation.core.satellite import Satellite
from constellation.core.configuration import Configuration
from powerSupplyControl.Keithley6517 import KeithleySMU6517Series
import logging

METRICS_PERIOD = 5.0


class Keithley_Satellite(Satellite):
    """Constellation Satellite to control a Keithley6517."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.config_file = "./python/satellites/powerSupplyControl/config_keithley_SourceVmeasureI.yaml"
        with open(self.config_file, "r") as configFile:
            self.yaml_config = yaml.safe_load(configFile)

        self.current_publish_interval = self.yaml_config["CurrentPublish"]["Interval"]

    def do_initializing(self, config: Configuration) -> str:
        """Method for the device-specific code of 'initializing' transition.

        This should set configuration variables.

        """
        super().do_initializing(config)
        self.log.info("Loading Keithley configuration file " + self.config_file + ".")

        # Create the device, which loads the config
        self.device = KeithleySMU6517Series(self.yaml_config)

        # Voltage from config file
        self.v_step = self.yaml_config["SetVoltage"]["V_Step"]
        self.v_set = self.yaml_config["SetVoltage"]["V_Set"]
        return "Initialized."

    def do_launching(self, payload: any) -> str:
        """Prepare Satellite for data acquistions."""
        """Callback method for the 'prepare' transition of the FSM.
        """
        super().do_launching(payload)
        # Ramp up, and already here start logging currents, really.
        self.log.info(
            "Launching Keithley satellite. Activating output and ramping up voltage to "
            + str(self.v_set)
            + " V in steps of "
            + str(self.v_step)
            + " V."
        )

        # Note:
        # Before any settings are changed, the data acquisition from the device needs to be paused
        # (as we can't have two processes writing to the Keithley at the same time).
        # Pausing can be done by calling pauseAcq(), unpausing by calling resumeAcq().

        # Ramp to safe voltage before anything else
        # Need to disable output
        self.device.disable_output()
        self.device.set_voltage(
            self.device._SafeLevelSource, unit="V"
        )  # TODO: not so elegant to have V here
        self.device.enable_output()

        # ramp to V_Set
        self.device.ramp_v(self.v_set, self.v_step, "V")

        # How to stop it, though?
        self.schedule_metric(
            "Current",
            self.device.get_current,
            interval=METRICS_PERIOD,
        )

        # How to stop it, though?
        # self.schedule_metric(
        #    "Voltage",
        #    self.device.get_voltage,
        #    interval=METRICS_PERIOD,
        # )
        return "Launched."

    def do_landing(self, payload: any) -> str:
        """Return Satellite to Initialized state."""
        super().do_landing(payload)
        self.log.info(
            "Landing Keithley satellite. Ramping down voltage to safe level of "
            + str(self.device._SafeLevelSource)
            + " V."
        )
        # TODO: Stop current-publishing thread
        self.device.ramp_v(self.device._SafeLevelSource, self.v_step, "V")
        return "Landed."

    def do_stopping(self, payload: any):
        """Stop the data acquisition."""
        super().do_stopping()
        # Stops acquisition... Do we do anything? I'd say no. Keep logging
        self.log.info("Acquisition stopped, but we keep sourcing the voltage.")
        return "Acquisition stopped."

    def do_starting(self, payload: any) -> str:
        """Final preparation for acquisition."""
        super().do_starting()
        # Stops acquisition... Do we do anything? I'd say no. Keep logging
        self.log.info("Acquisition starting, but we just keep sourcing the voltage.")
        return "Finished preparations, starting."

    def do_run(self, payload: any) -> str:
        """The acquisition event loop."""
        super().do_run(payload)
        # Doing nothing special here
        self.log.info("Keithley satellite running, publishing currents")

    def do_interrupting(self):
        """Interrupt data acquisition and move to Safe state.

        Defaults to calling the stop and land handlers.
        """
        super().do_interrupting()
        self.log.warning(
            "Keithley satellite interrupted. Ramping down voltage to safe level of "
            + str(self.device._SafeLevelSource)
            + " V."
        )
        # Stop current-publishing thread
        self.publish_current = False
        if self.current_stat_thread.is_alive():
            self.current_stat_thread.join()
        # Ramp down
        self.device.ramp_v(self.device._SafeLevelSource, self.v_step, "V")
        return "Interrupted."

    def on_failure(self):
        """Callback method for the 'on_failure' transition of the FSM."""
        super().on_failure()
        # self.logstatspub.sendLog(
        #    LogLevels.ERROR,
        #    "Keithley satellite failure. Ramping down voltage to safe level of "
        #    + str(self.device._SafeLevelSource)
        #    + " V.",
        # )
        # Stop current-publishing thread
        self.publish_current = False
        if self.current_stat_thread.is_alive():
            self.current_stat_thread.join()
        # Ramp down
        self.device.ramp_v(self.device._SafeLevelSource, self.v_step, "V")

    def on_recover(self):
        """Callback method for the 'on_recover' transition of the FSM."""
        pass

    def on_reset(self):
        """Callback method for the 'on_reset' transition of the FSM."""
        # self.logstatspub.sendLog(
        #    LogLevels.INFO, "Resetting and removing Keithley device."
        # )
        self.device.disconnect()


# -------------------------------------------------------------------------


def main(args=None):
    """Start the base Satellite server."""
    import argparse
    import coloredlogs

    parser = argparse.ArgumentParser(description=main.__doc__)
    parser.add_argument("--log-level", default="info")
    parser.add_argument("--cmd-port", type=int, default=23999)
    parser.add_argument("--mon-port", type=int, default=55556)
    parser.add_argument("--hb-port", type=int, default=61234)
    parser.add_argument("--interface", type=str, default="*")
    parser.add_argument("--name", type=str, default="satellite_demo")
    parser.add_argument("--group", type=str, default="constellation")
    args = parser.parse_args(args)

    # set up logging
    logger = logging.getLogger(args.name)
    coloredlogs.install(level=args.log_level.upper(), logger=logger)

    logger.info("Starting up satellite!")
    # start server with remaining args
    s = Keithley_Satellite(
        name=args.name,
        group=args.group,
        cmd_port=args.cmd_port,
        hb_port=args.hb_port,
        mon_port=args.mon_port,
        interface=args.interface,
    )
    s.run_satellite()


if __name__ == "__main__":
    main()
