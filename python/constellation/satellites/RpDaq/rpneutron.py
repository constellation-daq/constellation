#!/usr/bin/env python3

"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides an implementation for a Constellation Satellite on a
RedPitaya device.
"""

import mmap
import numpy as np
from .rpsatellite import RedPitayaSatellite, axi_regset_start_stop
from constellation.core.satellite import SatelliteArgumentParser
from constellation.core.base import setup_cli_logging
import rp

axi_regset_config = np.dtype(
    [
        ("data_type", "uint32"),
        ("active_channels", "uint32"),
        ("use_test_pulser", "uint32"),
        ("running_sum_integration_time", "uint32"),
        ("averaging_integration_time", "uint32"),
        ("trigger_level", "uint32"),
        ("tot_ch0_1", "uint32"),
        ("tot_ch2_3", "uint32"),
        ("dist_ch0_1", "uint32"),
        ("dist_ch2_3", "uint32"),
        ("ratio", "uint32"),
    ]
)

axi_regset_readout = np.dtype(
    [
        ("data_type", "uint32"),
        ("active_channels", "uint32"),
        ("use_test_pulser", "uint32"),
        ("running_sum_integration_time", "uint32"),
        ("averaging_integration_time", "uint32"),
        ("trigger_level", "uint32"),
        ("tot_ch0_1", "uint32"),
        ("tot_ch2_3", "uint32"),
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

RP_CHANNELS = [rp.RP_CH_1, rp.RP_CH_2, rp.RP_CH_3, rp.RP_CH_4]


class RPNeutron(RedPitayaSatellite):
    """Constellation Satellite to control a RedPitaya for neutron event
    detection."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.device = "RedPitaya_125_14"
        self.axi_regset_config = axi_regset_config
        self.regset_readout = axi_regset_readout
        self.active_channels = RP_CHANNELS
        self.master = False

    def do_initializing(self, payload: any) -> str:
        self.master = self.config["master"]
        # Define axi array for custom start stop register
        axi_mmap0 = mmap.mmap(
            self.memory_file_handle,
            mmap.PAGESIZE,
            mmap.MAP_SHARED,
            mmap.PROT_READ | mmap.PROT_WRITE,
            offset=0x40001000,
        )
        axi_numpy_array0 = np.recarray(1, axi_regset_start_stop, buf=axi_mmap0)
        self.axi_array_contents0 = axi_numpy_array0[0]
        return super().do_initializing(payload)

    def write_start_stop_bit_to_FPGA(self, start_stop):
        self.axi_array_contents0.Externaltrigger = start_stop

    def do_stopping(self, payload: any):
        """Stop acquisition by writing to address."""
        if self.master:
            self.write_start_stop_bit_to_FPGA(0)
        return super().do_stopping(payload)

    def do_starting(self, payload: any) -> str:
        """Start acquisition by writing to address."""
        if self.master:
            self.write_start_stop_bit_to_FPGA(3)
        return super().do_starting(payload)


# -------------------------------------------------------------------------


def main(args=None):
    "Start a RedPitaya neutron detector DAQ satellite"
    parser = SatelliteArgumentParser(
        description=main.__doc__,
        epilog="This is a 3rd-party component of Constellation.",
    )
    # this sets the defaults for our Satellite
    parser.set_defaults(
        cmd_port=23999,
        mon_port=55556,
        hb_port=61234,
        data_port=55557,
    )
    args = vars(parser.parse_args(args))

    # set up logging
    setup_cli_logging(args["name"], args.pop("log_level"))

    # start server with remaining args
    s = RPNeutron(**args)
    s.run_satellite()


if __name__ == "__main__":
    main()
