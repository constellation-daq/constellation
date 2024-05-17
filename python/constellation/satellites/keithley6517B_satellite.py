"""This module provides a Constellation Satellite for controlling a Keithley power supply."""

from constellation.core.satellite import Satellite
from constellation.core.configuration import Configuration
from constellation.core.commandmanager import cscp_requestable
from constellation.core.cscp import CSCPMessage
from powerSupplyControl.Keithley6517 import KeithleySMU6517Series
import logging

from functools import partial


class Keithley_Satellite(Satellite):
    """Constellation Satellite to control a Keithley6517."""

    def do_initializing(self, config: Configuration) -> str:
        """Method for the device-specific code of 'initializing' transition.

        This should set configuration variables.

        """

        # Create the device, which loads the config
        self.device = KeithleySMU6517Series(self.config)

        # Voltage from config file
        self.v_step = self.config["voltage_step"]
        self.v_set = self.config["voltage_set"]
        self.settle_time = self.config["settle_time"]

        return "Initialized."

    def do_launching(self, payload: any) -> str:
        """Prepare Satellite for data acquistions."""
        """Callback method for the 'prepare' transition of the FSM.
        """
        # Ramp up, and already here start logging currents, really.
        self.log.info(
            "Launching Keithley satellite. Activating output and ramping up voltage to "
            + str(self.v_set)
            + " V in steps of "
            + str(self.v_step)
            + " V."
        )

        # Go to safe voltage before anything else
        # Need to disable output
        self.device.disable_output()
        self.device.set_voltage(
            self.device._SafeLevelSource, unit="V"
        )  # TODO: not so elegant to have V here
        self.device.enable_output()

        # ramp to V_Set
        self.device.ramp_v(self.v_set, self.v_step, "V", self.settle_time)

        # How to stop it, though?
        self.schedule_metric(
            "Current",
            partial(self.device.get_current_timestamp_voltage, "current"),
            interval=self.config["stat_publishing_interval"],
        )
        self.schedule_metric(
            "Voltage",
            partial(self.device.get_current_timestamp_voltage, "voltage"),
            interval=self.config["stat_publishing_interval"],
        )

        return "Launched."

    def do_landing(self, payload: any) -> str:
        """Return Satellite to Initialized state."""
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
        # Stops acquisition... Do we do anything? I'd say no. Keep logging
        self.log.info("Acquisition stopped, but we keep sourcing the voltage.")
        return "Acquisition stopped."

    def do_starting(self, payload: any) -> str:
        """Final preparation for acquisition."""
        # Stops acquisition... Do we do anything? I'd say no. Keep logging
        self.log.info("Acquisition starting, but we just keep sourcing the voltage.")
        return "Finished preparations, starting."

    def do_run(self, payload: any) -> str:
        """The acquisition event loop."""
        # Doing nothing special here
        self.log.info("Keithley satellite running, publishing currents")

    def do_interrupting(self):
        """Interrupt data acquisition and move to Safe state.

        Defaults to calling the stop and land handlers.
        """
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

    @cscp_requestable
    def ramp_voltage(self, request: CSCPMessage):
        """Ramp voltage to a given value. Function takes three arguments;
        value to ramp to (in volts), step size (in volts), and optionally settle time (in seconds).
        """
        paramList = request.payload
        target = paramList[0]
        step = paramList[1]
        settle_time = paramList[2]
        self.device.ramp_v(target, step, "V", settle_time)
        return "Ramping voltage to" + str(target) + " V", None, None

    def _ramp_voltage_is_allowed(self, request: CSCPMessage):
        """Allow in the states INIT and ORBIT, but not during RUN"""
        return self.fsm.current_state.id in ["INIT", "ORBIT"]

    @cscp_requestable
    def get_current(self, request: CSCPMessage):
        """Read the current current. Takes no parameters"""
        current = self.device.get_current_timestamp_voltage("current")
        return ("Current current is " + str(current[0]) + " " + current[1]), None, None

    def _get_current_is_allowed(self, request: CSCPMessage):
        """Allow in the states INIT, ORBIT, RUN, SAFE, ERROR"""
        return self.fsm.current_state.id in ["INIT", "ORBIT", "RUN", "SAFE", "ERROR"]


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
