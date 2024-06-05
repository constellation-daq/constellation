#!/usr/bin/env python3

"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides an implementation for a Constellation Satellite on a
RedPitaya device.
"""

import mmap
import os
import numpy as np
import rp

from constellation.core.satellite import SatelliteArgumentParser
from constellation.core.base import setup_cli_logging
from constellation.core.configuration import ConfigError
from .rpsatellite import RedPitayaSatellite, axi_regset_start_stop

RPG_CHANNELS = [rp.RP_CH_1, rp.RP_CH_2]

axi_regset_config = np.dtype(
    [
        ("data_type", "uint32"),
        ("active_channels", "uint32"),
        ("use_test_pulser", "uint32"),
        ("running_sum_integration_time", "uint32"),
        ("averaging_integration_time", "uint32"),
        ("trigger_level", "uint32"),
    ]
)

axi_regset_readout = np.dtype(
    [
        ("data_type", "uint32"),
        ("active_channels", "uint32"),
        ("use_test_pulser", "uint32"),
        ("shiftSamplesExp", "uint32"),
        ("averaging_integration_time", "uint32"),
        ("trigger_level", "uint32"),
        ("trigger_per_s_ch0", "uint32"),
        ("trigger_per_s_ch1", "uint32"),
        ("mean_of_signal_ch0", "uint32"),
        ("mean_of_signal_ch1", "uint32"),
        ("mean_error_of_signal_ch0", "uint32"),
        ("mean_error_of_signal_ch1", "uint32"),
        ("total_number_of_triggers_ch0", "uint32"),
        ("total_number_of_triggers_ch1", "uint32"),
    ]
)


class RPGamma(RedPitayaSatellite):
    """Constellation Satellite to control a RedPitaya for gamma event
    detection."""

    def __init__(self, *args, **kwargs):

        self.device = "RedPitaya_250_12"
        self.axi_regset_config = axi_regset_config
        self.regset_readout = axi_regset_readout
        self.active_channels = RPG_CHANNELS
        super().__init__(*args, **kwargs)

    def do_initializing(self, payload: any) -> str:
        """Initialize satellite. Change the FPGA image and set register
        values."""
        try:
            for ch in RPG_CHANNELS:
                rp.rp_AcqSetAC_DC(ch, rp.RP_DC)
                rp.rp_AcqSetGain(
                    ch, rp.RP_LOW
                )  # They are not well documented and easy to confuse.
                # I believe these are the correct values set.

        except (ConfigError, OSError) as e:
            self.log.error("Error configuring device. %s", e)
        return super().do_initializing(payload)

    def do_starting(self, payload: any) -> str:

        self.reset()

        memory_file_handle = os.open("/dev/mem", os.O_RDWR)
        axi_mmap0 = mmap.mmap(
            fileno=memory_file_handle, length=mmap.PAGESIZE, offset=0x40600000
        )
        axi_numpy_array0 = np.recarray(1, axi_regset_start_stop, buf=axi_mmap0)
        axi_array_contents0 = axi_numpy_array0[0]

        axi_array_contents0.Externaltrigger = self.config["data_type"] + 32
        return "Started"

    def do_stopping(self, payload: any):
        memory_file_handle = os.open("/dev/mem", os.O_RDWR)
        axi_mmap0 = mmap.mmap(
            fileno=memory_file_handle, length=mmap.PAGESIZE, offset=0x40600000
        )
        axi_numpy_array0 = np.recarray(1, axi_regset_start_stop, buf=axi_mmap0)
        axi_array_contents0 = axi_numpy_array0[0]

        axi_array_contents0.Externaltrigger = 0
        return super().do_stopping(payload)


# -------------------------------------------------------------------------


def main(args=None):
    "Start a RedPitaya gamma detector DAQ satellite"

    parser = SatelliteArgumentParser(
        description=main.__doc__,
        epilog="This is a 3rd-party component of Constellation.",
    )
    # this sets the defaults for our Satellite
    parser.set_defaults(
        name=str(os.uname().nodename),
        cmd_port=23999,
        mon_port=55556,
        hb_port=61234,
        data_port=55557,
    )
    args = vars(parser.parse_args(args))

    # set up logging
    setup_cli_logging(args["name"], args.pop("log_level"))

    # start server with remaining args
    s = RPGamma(**args)
    s.run_satellite()


if __name__ == "__main__":
    main()
