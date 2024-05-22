#!/usr/bin/env python3

"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides an implementation for a Constellation Satellite on a
RedPitaya device.
"""

import logging
import mmap
import os
import coloredlogs
import numpy as np
from .rpsatellite import RedPitayaSatellite, axi_regset_start_stop

axi_regset_config = np.dtype(
    [
        ("data_type", "uint32"),
        ("active_channles", "uint32"),
        ("use_test_pulser", "uint32"),
        ("running_sum_Integration_time", "uint32"),
        ("averaging_Integration_time", "uint32"),
        ("trigger_level", "uint32"),
        ("ToT_ch0_1", "uint32"),
        ("ToT_ch2_3", "uint32"),
        ("dist_ch0_1", "uint32"),
        ("dist_ch2_3", "uint32"),
        ("ratio", "uint32"),
    ]
)

axi_regset_readout = np.dtype(
    [
        ("data_type", "uint32"),
        ("active_channles", "uint32"),
        ("use_test_pulser", "uint32"),
        ("running_sum_Integration_time", "uint32"),
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
    """Constellation Satellite to control a RedPitaya for neutron event
    detection."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.device = "RedPitaya_125_14"
        self.axi_regset_config = axi_regset_config
        self.regset_readout = axi_regset_readout
        self.master = False

    def do_initializing(self, payload: any) -> str:
        self.master = self.config["master"]
        return super().do_initializing(payload)

    def do_stopping(self, payload: any):
        """Stop acquisition by writing to address."""
        if self.master:
            memory_file_handle = os.open("/dev/mem", os.O_RDWR)
            axi_mmap0 = mmap.mmap(
                fileno=memory_file_handle, length=mmap.PAGESIZE, offset=0x40001000
            )

            axi_numpy_array0 = np.recarray(1, axi_regset_start_stop, buf=axi_mmap0)
            axi_array_contents0 = axi_numpy_array0[0]
            axi_array_contents0.Externaltrigger = (
                0  # Don'tOverride GPIO_N_0 to output ADC or DAC trigger
            )
        return super().do_stopping(payload)

    def do_starting(self, payload: any) -> str:
        """Start acquisition by writing to address."""
        self.reset()

        if self.master:
            memory_file_handle = os.open("/dev/mem", os.O_RDWR)
            axi_mmap0 = mmap.mmap(
                fileno=memory_file_handle, length=mmap.PAGESIZE, offset=0x40001000
            )
            axi_numpy_array0 = np.recarray(1, axi_regset_start_stop, buf=axi_mmap0)
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
    parser.add_argument("--name", type=str, default=str(os.uname().nodename))
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
