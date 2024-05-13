#!/usr/bin/env python3
"""This module provides a Constellation Satellite for controlling a Keithley power supply."""


import yaml

from constellation.core.satellite import Satellite
from constellation.core.configuration import ConfigError, Configuration
from powerSupplyControl.Keithley2400 import KeithleySMU2400Series
import time
import logging


class Keithley_Satellite(Satellite):
    """Constellation Satellite to control a Keithley2410."""
    def __init__(self, cmd_port, hb_port, log_port, config_file):
        super().__init__(cmd_port, hb_port, log_port)
        self.config_file = config_file
        with open(self.config_file, 'r') as configFile:
            self.yaml_config = yaml.safe_load(configFile)
        
        self.current_publish_interval = self.yaml_config["CurrentPublish"]["Interval"]
    
    def do_initializing(self, config: Configuration) -> str:
        """Method for the device-specific code of 'initializing' transition.

        This should set configuration variables.

        """
        return "Initialized."
        
    def do_launching(self, payload: any) -> str:
        """Prepare Satellite for data acquistions."""
        return "Launched."
        
    def do_landing(self, payload: any) -> str:
        """Return Satellite to Initialized state."""
        return "Landed."
        
    def do_stopping(self, payload: any):
        """Stop the data acquisition."""
        return "Acquisition stopped."
        
    def do_run(self, payload: any) -> str:
        """The acquisition event loop.

        This method will be started by the Satellite and run in a thread. It
        therefore needs to monitor the self.stop_running Event and close itself
        down if the Event is set.

        NOTE: This method is not inherently thread-safe as it runs in the
        context of the Satellite and can modify data accessible to the main
        thread. However, the state machine can effectively act as a lock and
        prevent competing access to the same objects while in RUNNING state as
        long as care is taken in the implementation.

        The state machine itself uses the RTC model by default (see
        https://python-statemachine.readthedocs.io/en/latest/processing_model.html?highlight=thread)
        which should make the transitions themselves safe.

        """
        # the stop_running Event will be set from outside the thread when it is
        # time to close down.
        while not self._state_thread_evt.is_set():
            time.sleep(0.2)
        return "Finished acquisition."
        
    def do_interrupting(self):
        """Interrupt data acquisition and move to Safe state.

        Defaults to calling the stop and land handlers.
        """
        self.do_stopping()
        self.do_landing()
        return "Interrupted."

    def on_load(self):
        self.logstatspub.sendLog(LogLevels.INFO, "Loading Keithley configuration file " + self.config_file + ".")
        
        # Create the device, which loads the config
        self.device = KeithleySMU2400Series(self.yaml_config)

        # Voltage from config file
        self.v_step = self.yaml_config["SetVoltage"]["V_Step"]
        self.v_set = self.yaml_config["SetVoltage"]["V_Set"]

    def on_unload(self):
        """Callback method for the 'deinitialize' transition of the FSM.

        Undo actions from 'do_initialize'
        """
        # Unload config, reset everything to default
        self.logstatspub.sendLog(LogLevels.INFO, "Resetting and removing Keithley device.")
        self.device.disconnect()
        self.device = None
        self.current_stat_thread = None

    def on_launch(self):
        """Callback method for the 'prepare' transition of the FSM.
        """
        super().on_launch()
        # Ramp up, and already here start logging currents, really.
        self.logstatspub.sendLog(
            LogLevels.INFO, "Launching Keithley satellite. Activating output and ramping up voltage to " + str(self.v_set) + " V in steps of " + str(self.v_step) + " V.")

        # Note:
        # Before any settings are changed, the data acquisition from the device needs to be paused
        # (as we can't have two processes writing to the Keithley at the same time).
        # Pausing can be done by calling pauseAcq(), unpausing by calling resumeAcq().

        # Ramp to safe voltage before anything else
        # Need to disable output
        self.device.disable_output()
        self.device.set_voltage(self.device._SafeLevelSource, unit='V') #TODO: not so elegant to have V here
        self.device.enable_output()

        # ramp to V_Set
        self.device.ramp_v(self.v_set, self.v_step, 'V')

        # Start publishing the current
        self.publish_current = True
        self.current_stat_thread = threading.Thread(target=self.pub_current_stat, args=(self.current_publish_interval,))
        self.current_stat_thread.start()


    def on_land(self):
        """Callback method for the 'unprepare' transition of the FSM.
        """
        super().on_land()
        # Ramp down, stop publishing currents
        self.logstatspub.sendLog(
            LogLevels.INFO, "Landing Keithley satellite. Ramping down voltage to safe level of " + str(self.device._SafeLevelSource) + " V.")
        # Stop current-publishing thread
        self.publish_current = False
        if self.current_stat_thread.is_alive():
            self.current_stat_thread.join()
        # ramp to safe
        self.device.ramp_v(self.device._SafeLevelSource, self.v_step, 'V')


    def on_start(self):
        """Callback method for the 'start_run' transition of the FSM.

        This method needs to start acquisition on a new thread.
        """
        super().on_start()
        # Do nothing special, I would say. Just keep on logging

    def on_stop(self):
        """Callback method for the 'stop_run' transition of the FSM.

        This method needs to the stop acquisition on the other thread.
        """
        super().on_stop()
        # Stops acquisition... Do we do anything? I'd say no. Keep logging

    def do_run(self):
        # Doing nothing special here
        self.logstatspub.sendLog(LogLevels.INFO, "Keithley satellite running, publishing currents")

    def on_failure(self):
        """Callback method for the 'on_failure' transition of the FSM.
        """
        super().on_failure()
        self.logstatspub.sendLog(
            LogLevels.ERROR, "Keithley satellite failure. Ramping down voltage to safe level of " + str(self.device._SafeLevelSource) + " V.")
        # Stop current-publishing thread
        self.publish_current = False
        if self.current_stat_thread.is_alive():
            self.current_stat_thread.join()
        # Ramp down
        self.device.ramp_v(self.device._SafeLevelSource, self.v_step, 'V')

    def on_interrupt(self):
        """Callback method for the 'on_interrupt' transition of the FSM.
        """
        super().on_interrupt()
        self.logstatspub.sendLog(
            LogLevels.WARNING, "Keithley satellite interrupted. Ramping down voltage to safe level of " + str(self.device._SafeLevelSource) + " V.")
        # Stop current-publishing thread
        self.publish_current = False
        if self.current_stat_thread.is_alive():
            self.current_stat_thread.join()
        # Ramp down
        self.device.ramp_v(self.device._SafeLevelSource, self.v_step, 'V')

    def on_recover(self):
        """Callback method for the 'on_recover' transition of the FSM.
        """
        pass

    def on_reset(self):
        """Callback method for the 'on_reset' transition of the FSM.
        """
        self.logstatspub.sendLog(LogLevels.INFO, "Resetting and removing Keithley device.")
        self.device.disconnect()

    # Publishes the current reading
    def pub_current_stat(self, interval):
        while self.publish_current:
            self.logstatspub.sendStats("CURRENT", eddaEnums.DataType.FLOAT, self.device.get_current()[0])
            time.sleep(interval)


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

