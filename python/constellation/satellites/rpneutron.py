#!/usr/bin/env python3

"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides an implementation for a Constellation Satellite on a RedPitaya device.
"""

import logging
import mmap
import os
import time

import coloredlogs
import numpy as np

from constellation.core.configuration import ConfigError
from .rpsatellite import RedPitayaSatellite, axi_gpio_regset_start_stop

axi_gpio_regset_config = np.dtype(
    [
        ("data_type", "uint32"),
        ("active_channles", "uint32"),
        ("use_test_pulser", "uint32"),
        ("runnint_sum_Integration_time", "uint32"),
        ("averaging_Integration_time", "uint32"),
        ("trigger_level", "uint32"),
        ("ToT_ch0_1", "uint32"),
        ("ToT_ch2_3", "uint32"),
        ("dist_ch0_1", "uint32"),
        ("dist_ch2_3", "uint32"),
        ("ratio", "uint32"),
        ("trigger_per_s_ch0", "uint32"),
        ("trigger_per_s_ch1", "uint32"),
        ("trigger_per_s_ch2", "uint32"),
        ("trigger_per_s_ch3", "uint32"),
        ("mean_of_signal_ch0", "uint32"),
        ("mean_of_signal_ch1", "uint32"),
        ("mean_of_signal_ch2", "uint32"),
        ("mean_of_signal_ch3", "uint32"),
        ("mean_error_of_signal_ch0", "uint32"),
        ("mean_error_of_signal_ch1", "uint32"),
        ("mean_error_of_signal_ch2", "uint32"),
        ("mean_error_of_signal_ch3", "uint32"),
        ("total_number_of_triggers_ch0", "uint32"),
        ("total_number_of_triggers_ch1", "uint32"),
        ("total_number_of_triggers_ch2", "uint32"),
        ("total_number_of_triggers_ch3", "uint32"),
        ("total_numbers_of_over_threshold_triggers_ch0", "uint32"),
        ("total_numbers_of_over_threshold_triggers_ch1", "uint32"),
        ("total_numbers_of_over_threshold_triggers_ch2", "uint32"),
        ("total_numbers_of_over_threshold_triggers_ch3", "uint32"),
        ("total_numbers_of_over_ToT_triggers_ch0", "uint32"),
        ("total_numbers_of_over_ToT_triggers_ch1", "uint32"),
        ("total_numbers_of_over_ToT_triggers_ch2", "uint32"),
        ("total_numbers_of_over_ToT_triggers_ch3", "uint32"),
        ("total_numbers_of_over_ratio_triggers_ch0", "uint32"),
        ("total_numbers_of_over_ratio_triggers_ch1", "uint32"),
        ("total_numbers_of_over_ratio_triggers_ch2", "uint32"),
        ("total_numbers_of_over_ratio_triggers_ch3", "uint32"),
        ("total_numbers_of_over_distance_triggers_ch0", "uint32"),
        ("total_numbers_of_over_distance_triggers_ch1", "uint32"),
        ("total_numbers_of_over_distance_triggers_ch2", "uint32"),
        ("total_numbers_of_over_distance_triggers_ch3", "uint32"),
        ("minValueOut_ch0", "uint32"),
        ("mintTOut_ch0", "uint32"),
        ("totalTOut_ch0", "uint32"),
        ("minValueOut_ch1", "uint32"),
        ("mintTOut_ch1", "uint32"),
        ("totalTOut_ch1", "uint32"),
        ("minValueOut_ch2", "uint32"),
        ("mintTOut_ch2", "uint32"),
        ("totalTOut_ch2", "uint32"),
        ("minValueOut_ch3", "uint32"),
        ("mintTOut_ch3", "uint32"),
        ("totalTOut_ch3", "uint32"),
    ]
)

axi_gpio_regset_readout = np.dtype(
    [
        ("data_type", "uint32"),
        ("active_channles", "uint32"),
        ("use_test_pulser", "uint32"),
        ("runnint_sum_Integration_time", "uint32"),
        ("averaging_Integration_time", "uint32"),
        ("trigger_level", "uint32"),
        ("ToT_ch0_1", "uint32"),
        ("ToT_ch2_3", "uint32"),
        ("dist_ch0_1", "uint32"),
        ("dist_ch2_3", "uint32"),
        ("ratio", "uint32"),
        ("trigger_per_s_ch0", "uint32"),
        ("trigger_per_s_ch1", "uint32"),
        ("trigger_per_s_ch2", "uint32"),
        ("trigger_per_s_ch3", "uint32"),
        ("mean_of_signal_ch0", "uint32"),
        ("mean_of_signal_ch1", "uint32"),
        ("mean_of_signal_ch2", "uint32"),
        ("mean_of_signal_ch3", "uint32"),
        ("mean_error_of_signal_ch0", "uint32"),
        ("mean_error_of_signal_ch1", "uint32"),
        ("mean_error_of_signal_ch2", "uint32"),
        ("mean_error_of_signal_ch3", "uint32"),
        ("total_number_of_triggers_ch0", "uint32"),
        ("total_number_of_triggers_ch1", "uint32"),
        ("total_number_of_triggers_ch2", "uint32"),
        ("total_number_of_triggers_ch3", "uint32"),
        ("total_numbers_of_over_threshold_triggers_ch0", "uint32"),
        ("total_numbers_of_over_threshold_triggers_ch1", "uint32"),
        ("total_numbers_of_over_threshold_triggers_ch2", "uint32"),
        ("total_numbers_of_over_threshold_triggers_ch3", "uint32"),
        ("total_numbers_of_over_ToT_triggers_ch0", "uint32"),
        ("total_numbers_of_over_ToT_triggers_ch1", "uint32"),
        ("total_numbers_of_over_ToT_triggers_ch2", "uint32"),
        ("total_numbers_of_over_ToT_triggers_ch3", "uint32"),
        ("total_numbers_of_over_ratio_triggers_ch0", "uint32"),
        ("total_numbers_of_over_ratio_triggers_ch1", "uint32"),
        ("total_numbers_of_over_ratio_triggers_ch2", "uint32"),
        ("total_numbers_of_over_ratio_triggers_ch3", "uint32"),
        ("total_numbers_of_over_distance_triggers_ch0", "uint32"),
        ("total_numbers_of_over_distance_triggers_ch1", "uint32"),
        ("total_numbers_of_over_distance_triggers_ch2", "uint32"),
        ("total_numbers_of_over_distance_triggers_ch3", "uint32"),
        ("minValueOut_ch0", "uint32"),
        ("mintTOut_ch0", "uint32"),
        ("totalTOut_ch0", "uint32"),
        ("minValueOut_ch1", "uint32"),
        ("mintTOut_ch1", "uint32"),
        ("totalTOut_ch1", "uint32"),
        ("minValueOut_ch2", "uint32"),
        ("mintTOut_ch2", "uint32"),
        ("totalTOut_ch2", "uint32"),
        ("minValueOut_ch3", "uint32"),
        ("mintTOut_ch3", "uint32"),
        ("totalTOut_ch3", "uint32"),
        ("Data_value_ch0", "uint32"),
        ("Data_value_ch1", "uint32"),
        ("Data_value_ch2", "uint32"),
        ("Data_value_ch3", "uint32"),
    ]
)


class RPNeutron(RedPitayaSatellite):
    """Constellation Satellite to control a RedPitaya for neutron event detection."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.device = "RedPitaya_125_12"
        self._regset_readout = axi_gpio_regset_readout
        self.master = False

    def do_initializing(self, payload: any) -> str:
        """Initialize satellite. Change the FPGA image and set register values."""
        try:
            # os.system('cat /root/Stopwatch.bit > /dev/xdevcfg')    # OS 1.04 or older

            bin_file = self.config["bin_file"]
            command = "/opt/redpitaya/bin/fpgautil -b " + bin_file
            if os.system(command) != 0:  # OS 2.00 and above
                msg = "System command failed."
                raise ConfigError(msg)
            time.sleep(5)

            memory_file_handle = os.open("/dev/mem", os.O_RDWR)
            axi_mmap = mmap.mmap(
                fileno=memory_file_handle,
                length=mmap.PAGESIZE,
                offset=self.config["offset"],
            )
            axi_numpy_array = np.recarray(1, axi_gpio_regset_config, buf=axi_mmap)
            axi_array_contents = axi_numpy_array[0]

            axi_array_contents.active_channles = self.config[
                "channels"
            ]  # Start Channels

            axi_array_contents.runnint_sum_Integration_time = self.config[
                "running_sum_Integration_time"
            ]  # Set 64 samples runnint_sum_Integration_time on all channels
            axi_array_contents.averaging_Integration_time = self.config[
                "averaging_Integration_time"
            ]  # Sets averaging_Integration time To 16383 on all channels
            axi_array_contents.trigger_level = self.config[
                "trigger_level"
            ]  # Sets trigger level =6 on all channels
            axi_array_contents.ToT_ch0_1 = self.config[
                "time_over_threshold_ch0_1"
            ]  # Sets TOT=50 on channel 0-1
            axi_array_contents.ToT_ch0_1 = self.config[
                "time_over_threshold_ch2_3"
            ]  # Sets TOT=50 on channel 2-3
            axi_array_contents.dist_ch0_1 = self.config[
                "dist_ch0_1"
            ]  # Sets dist=625 on channel 0-1
            axi_array_contents.dist_ch2_3 = self.config[
                "dist_ch2_3"
            ]  # Sets dist=625 on channel 2-3
            axi_array_contents.ratio = self.config[
                "ratio"
            ]  # Sets ratio to 2 on all channels

            axi_array_contents.use_test_pulser = self.config[
                "test_pulser_rate"
            ]  # Set test pulser active

            axi_array_contents.data_type = self.config["data_type"]
            self.master = self.config["master"]

            # Setup metrics
            if self.config["read_gpio"]:
                self.schedule_metric(
                    self.get_analog_gpio_pins.__name__,
                    self.get_analog_gpio_pins,
                    self.config["gpio_poll_rate"],
                )
                self.schedule_metric(
                    self.get_digital_gpio_pins.__name__,
                    self.get_digital_gpio_pins,
                    self.config["gpio_poll_rate"],
                )
            self.schedule_metric(
                self.get_cpu_temperature.__name__,
                self.get_cpu_temperature,
                self.config["metrics_poll_rate"],
            )
            self.schedule_metric(
                self.get_cpu_load.__name__,
                self.get_cpu_load,
                self.config["metrics_poll_rate"],
            )
            self.schedule_metric(
                self.get_memory_load.__name__,
                self.get_memory_load,
                self.config["metrics_poll_rate"],
            )
            self.schedule_metric(
                self.get_network_speeds.__name__,
                self.get_network_speeds,
                self.config["metrics_poll_rate"],
            )
            self.schedule_metric(
                self.read_registers.__name__,
                self.read_registers,
                self.config["metrics_poll_rate"],
            )
        except (ConfigError, OSError) as e:
            self.log.error("Error configuring device. %s", e)

        return super().do_initializing(payload)

    def do_stopping(self, payload: any):
        """Stop acquisition by writing to address."""
        if self.master:
            memory_file_handle = os.open("/dev/mem", os.O_RDWR)
            axi_mmap0 = mmap.mmap(
                fileno=memory_file_handle, length=mmap.PAGESIZE, offset=0x40001000
            )

            axi_numpy_array0 = np.recarray(1, axi_gpio_regset_start_stop, buf=axi_mmap0)
            axi_array_contents0 = axi_numpy_array0[0]
            axi_array_contents0.Externaltrigger = (
                0  # Don'tOverride GPIO_N_0 to output ADC or DAC trigger
            )
        return super().do_stopping(payload)

    def do_starting(self, payload: any) -> str:
        """Start acquisition by writing to address."""
        if self.master:
            memory_file_handle = os.open("/dev/mem", os.O_RDWR)
            axi_mmap0 = mmap.mmap(
                fileno=memory_file_handle, length=mmap.PAGESIZE, offset=0x40001000
            )
            axi_numpy_array0 = np.recarray(1, axi_gpio_regset_start_stop, buf=axi_mmap0)
            axi_array_contents0 = axi_numpy_array0[0]
            axi_array_contents0.Externaltrigger = (
                3  # Override GPIO_N_0 to output ADC or DAC trigger
            )
        return super().do_starting(payload)


# -------------------------------------------------------------------------


def main(args=None):
    "Start a RedPitaya satellite"
    import argparse

    parser = argparse.ArgumentParser(description=main.__doc__)
    parser.add_argument("--log-level", default="info")
    parser.add_argument("--cmd-port", type=int, default=23999)
    parser.add_argument("--mon-port", type=int, default=55556)
    parser.add_argument("--hb-port", type=int, default=61234)
    parser.add_argument("--data-port", type=int, default=55557)
    parser.add_argument("--interface", type=str, default="*")
    parser.add_argument("--name", type=str, default="RedPitaya_neutron_sender")
    parser.add_argument("--group", type=str, default="constellation")
    args = parser.parse_args(args)

    # set up logging
    logger = logging.getLogger(args.name)
    coloredlogs.install(level=args.log_level.upper(), logger=logger)

    logger.info("Starting up satellite!")

    # start server with remaining args
    s = RPNeutron(
        name=args.name,
        group=args.group,
        cmd_port=args.cmd_port,
        hb_port=args.hb_port,
        mon_port=args.mon_port,
        data_port=args.data_port,
        interface=args.interface,
    )

    s.run_satellite()


if __name__ == "__main__":
    main()
