#!/usr/bin/env python3

"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides an implementation for a Constellation Satellite on a RedPitaya device.
"""

import ctypes
import logging
import mmap
import os
import time

import coloredlogs
import numpy as np
import rp

from .cmdp import MetricsType
from .commandmanager import cscp_requestable
from .confighandler import ConfigError
from .cscp import CSCPMessage
from .datasender import DataSender
from .monitoring import schedule_metric

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

axi_gpio_regset_start = np.dtype([("Externaltrigger", "uint32")])

axi_gpio_regset_stop = np.dtype([("Externaltrigger", "uint32")])

axi_gpio_regset_reset = np.dtype([("data_type", "uint32")])

BUFFER_SIZE = 16384
RP_CHANNELS = [rp.RP_CH_4, rp.RP_CH_3, rp.RP_CH_2, rp.RP_CH_1]

METRICS_PERIOD = 10


class RedPitayaSatellite(DataSender):
    """Constellation Satellite to control a RedPitaya."""

    def __init__(
        self,
        *args,
        **kwargs,
    ):
        super().__init__(*args, **kwargs)
        self._active_channels = []
        self._buffer = []
        self.prev_cpu_idle, self.prev_cpu_time = self._get_cpu_times()
        self.prev_tx = int(
            self._get_val_from_file("/sys/class/net/eth0/statistics/tx_bytes")
        )
        self.prev_rx = int(
            self._get_val_from_file("/sys/class/net/eth0/statistics/rx_bytes")
        )
        self._readpos = None
        self._writepos = None
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

            # Track active channels
            for idx, val in enumerate(format(self.config["channels"], "04b")):
                if int(val):
                    self._active_channels.append(RP_CHANNELS[idx])
                    self._buffer.append(rp.i16Buffer(BUFFER_SIZE))

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

            rp.rp_Init()
        except (ConfigError, OSError) as e:
            self.log.error("Error configuring device. %s", e)
        return super().do_initializing(payload)

    def do_launching(self, payload: any) -> str:
        """Callback method for the 'prepare' transition of the FSM."""
        self.log.info("Launching RedPitaya satellite. Activating ACQ.")

        return super().do_launching(payload)

    def do_landing(self, payload: any) -> str:
        self.log.info("Landing Red Pitaya satellite.")
        return super().do_landing(payload)

    def do_stopping(self, payload: any):
        """Stop acquisition by writing to address."""
        if self.master:
            memory_file_handle = os.open("/dev/mem", os.O_RDWR)
            axi_mmap0 = mmap.mmap(
                fileno=memory_file_handle, length=mmap.PAGESIZE, offset=0x40001000
            )

            axi_numpy_array0 = np.recarray(1, axi_gpio_regset_stop, buf=axi_mmap0)
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
            axi_numpy_array0 = np.recarray(1, axi_gpio_regset_start, buf=axi_mmap0)
            axi_array_contents0 = axi_numpy_array0[0]
            axi_array_contents0.Externaltrigger = (
                3  # Override GPIO_N_0 to output ADC or DAC trigger
            )
        return super().do_starting(payload)

    def do_run(self, payload):
        """Run the satellite. Collect data from buffers and send it."""
        self.log.info("Red Pitaya satellite running, publishing events.")

        self._readpos = self.get_write_pointer()
        while not self._state_thread_evt.is_set():
            # Main DAQ-loop
            payload = self.get_data()

            if payload is None:
                continue

            # Include data type as part of meta
            meta = {
                "dtype": f"{payload.dtype}",
            }

            # Format payload to serializable
            self.data_queue.put((payload.tolist(), meta))

        return "Finished acquisition"

    def do_interrupting(self):
        return super().do_interrupting()

    @cscp_requestable
    def get_device(self, _request: CSCPMessage):
        """Get name of device."""
        return (
            "RedPitaya_125_14",
            None,
            None,
        )  # NOTE: Placeholder: should be more detailed

    @cscp_requestable
    def get_registers(self, _request: CSCPMessage):
        """Get values stored in registers in axi_gpio_regset_readout."""
        return (
            str(self.read_registers()),
            None,
            None,
        )  # NOTE: Not sure if this is how we should do it

    def sample_raw(self, channel, buffer, chunk):
        """Sample data from given channel."""

        rp.rp_AcqGetDataRaw(channel, self._readpos, chunk, buffer.cast())

        data_raw = np.zeros(chunk, dtype=int)

        for idx in range(0, chunk, 1):
            data_raw[idx] = buffer[idx]

        return data_raw

    def get_data(
        self,
    ):  # TODO: Check performance. This was lifted from the redpitaya examples
        """Sample every buffer channel and return raw data in numpy array."""

        # Obtain to which point the buffer has written
        self._writepos = self.get_write_pointer()

        # Skip sampling if we haven't moved
        if self._readpos == self._writepos:
            return None

        # Check if buffer has cycled
        if self._writepos < self._readpos:
            cycled = True
        else:
            cycled = False

        # Sample data for every channel and convert to list of numpy arrays
        data = []
        for _, channel in enumerate(self._active_channels):
            # Buffer all appended data for channel before adding it together
            buffer = []
            # Append last part of buffer before resetting
            if cycled:
                buffer.append(
                    self._sample_raw32(
                        start=self._readpos,
                        stop=BUFFER_SIZE,
                        channel=channel,
                    ),
                )
                self._readpos = 0

            buffer.append(
                self._sample_raw32(
                    start=self._readpos,
                    stop=self._writepos,
                    channel=channel,
                ),
            )
            data.append(np.concatenate(buffer))

        # Update readpointer
        self._readpos = self._writepos

        # data = np.vstack(data, dtype=int).transpose().flatten()
        return np.array(data, dtype=np.int32)

    def _sample_raw32(self, start: int = 0, stop: int = 16384, channel: rp.RP_CHAN = 1):
        """Read out data in 32 bit form."""

        class Array(ctypes.Structure):
            """Define the struct in Python"""

            _fields_ = [("data", ctypes.POINTER(ctypes.c_uint32))]

        # Load the shared library
        lib = ctypes.CDLL("/root/read_data32.so")

        # Define the argument and return types of the function
        lib.readData.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
        lib.readData.restype = Array

        # Define register offset depending onc channel
        if channel == rp.RP_CH_1:
            offset = 1074855936
        elif channel == rp.RP_CH_2:
            offset = 1074921472
        elif channel == rp.RP_CH_3:
            offset = 1075904512
        elif channel == rp.RP_CH_4:
            offset = 1075970048

        # Call the C function
        result = lib.readData(0, 16384, offset)
        lib.freeData(result.data)

        # Convert the result to a NumPy array
        data_array = np.ctypeslib.as_array(result.data, shape=(16384,))[start:stop]

        return data_array

    def get_write_pointer(self):
        """Obtain write pointer"""
        return rp.rp_AcqGetWritePointer()[1]

    def read_registers(self):
        """
        Reads the stored values of the axi_gpio_regset and returns a
        tuple of their respective names and values.
        """
        memory_file_handle = os.open("/dev/mem", os.O_RDWR)
        axi_mmap = mmap.mmap(
            fileno=memory_file_handle, length=mmap.PAGESIZE, offset=0x40600000
        )
        axi_numpy_array = np.recarray(1, axi_gpio_regset_readout, buf=axi_mmap)
        axi_array_contents = axi_numpy_array[0]
        names = [field[0] for field in axi_gpio_regset_readout.descr]

        ret = {}
        for name, value in zip(names, axi_array_contents):
            ret[name] = value
        return ret

    @schedule_metric(handling=MetricsType.LAST_VALUE, interval=METRICS_PERIOD)
    def get_cpu_temperature(self):
        """Obtain temperature of CPU."""
        paths = (
            "/sys/devices/soc0/axi/83c00000.xadc_wiz/iio:device1/in_temp0_offset",
            "/sys/devices/soc0/axi/83c00000.xadc_wiz/iio:device1/in_temp0_raw",
            "/sys/devices/soc0/axi/83c00000.xadc_wiz/iio:device1/in_temp0_scale",
        )

        offset = int(self._get_val_from_file(paths[0]))
        raw = int(self._get_val_from_file(paths[1]))
        scale = float(self._get_val_from_file(paths[2]))
        return ((float)(offset + raw)) * scale / 1000.0, "C"

    @schedule_metric(handling=MetricsType.LAST_VALUE, interval=METRICS_PERIOD)
    def get_cpu_load(self):
        """Estimate current CPU load and update previously saved CPU times."""
        idle_cpu_time, total_cpu_time = self._get_cpu_times()
        total_cpu_time2 = total_cpu_time - self.prev_cpu_time
        idle_cpu_time2 = idle_cpu_time - self.prev_cpu_idle
        utilization = ((total_cpu_time2 - idle_cpu_time2) * 100) / total_cpu_time2
        self.prev_cpu_time = total_cpu_time
        self.prev_cpu_idle = idle_cpu_time
        return utilization, "%"

    @schedule_metric(handling=MetricsType.LAST_VALUE, interval=METRICS_PERIOD)
    def get_memory_load(self):
        """Obtain current memory usage."""
        # Obtain memory info from file
        mem = self._get_val_from_file("/proc/meminfo").split("\n")
        tot_mem = mem[0].split(" ")[9]
        free_mem = mem[1].split(" ")[9]
        used_mem = tot_mem - free_mem

        return used_mem, "kb"

    @schedule_metric(handling=MetricsType.LAST_VALUE, interval=METRICS_PERIOD)
    def get_network_speeds(self):
        """Estimate current network speeds."""
        tx_bytes = int(
            self._get_val_from_file("/sys/class/net/eth0/statistics/tx_bytes")
        )
        rx_bytes = int(
            self._get_val_from_file("/sys/class/net/eth0/statistics/rx_bytes")
        )

        tx_speed = (tx_bytes - self.prev_tx) / METRICS_PERIOD
        rx_speed = (rx_bytes - self.prev_rx) / METRICS_PERIOD

        self.prev_tx = tx_bytes
        self.prev_rx = rx_bytes

        return (tx_speed, rx_speed), "kb/s"

    def _get_cpu_times(self):
        """Obtain idle time and active time of CPU."""
        # Get the line containing total values of CPU time
        stat = self._get_val_from_file("/proc/stat").split("\n")[0].split(" ")[2:]

        idle_cpu_time = 0
        total_cpu_time = 0
        for idx, val in enumerate(stat):
            total_cpu_time += int(val)
            if idx == 3:
                idle_cpu_time = int(val)

        return idle_cpu_time, total_cpu_time

    def _get_val_from_file(self, path: str):
        """Fetch all information stored in file from path."""
        try:
            f = open(path, "r")
            var = f.read()
            f.close()
            return var
        except FileNotFoundError:
            self.log.warning("Failed to find path %s", path)


# -------------------------------------------------------------------------


def main(args=None):
    import argparse

    parser = argparse.ArgumentParser(description=main.__doc__)
    parser.add_argument("--log-level", default="info")
    parser.add_argument("--cmd-port", type=int, default=23999)
    parser.add_argument("--mon-port", type=int, default=55556)
    parser.add_argument("--hb-port", type=int, default=61234)
    parser.add_argument("--data-port", type=int, default=55557)
    parser.add_argument("--interface", type=str, default="*")
    parser.add_argument("--name", type=str, default="RedPitaya_data_sender")
    parser.add_argument("--group", type=str, default="constellation")
    args = parser.parse_args(args)

    # set up logging
    logger = logging.getLogger(args.name)
    coloredlogs.install(level=args.log_level.upper(), logger=logger)

    logger.info("Starting up satellite!")

    # start server with remaining args
    s = RedPitayaSatellite(
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
