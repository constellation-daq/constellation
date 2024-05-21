#!/usr/bin/env python3

"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides an implementation for a Constellation Satellite on a
RedPitaya device.
"""

import ctypes
import logging
import mmap
import os
import time

import re
import coloredlogs
import numpy as np
import rp

from constellation.core.commandmanager import cscp_requestable
from constellation.core.cscp import CSCPMessage
from constellation.core.datasender import DataSender
from constellation.core.configuration import ConfigError

axi_regset_start_stop = np.dtype([("Externaltrigger", "uint32")])

axi_regset_reset = np.dtype([("data_type", "uint32")])

axi_gpio_regset_pins = np.dtype(
    [
        ("dummy_1", "uint32"),
        ("dummy_2", "uint32"),
        ("dummy_3", "uint32"),
        ("dummy_4", "uint32"),
        ("dummy_5", "uint32"),
        ("dummy_6", "uint32"),
        ("dummy_7", "uint32"),
        ("dummy_8", "uint32"),
        ("n_pins", "uint32"),
        ("p_pins", "uint32"),
    ]
)

BUFFER_SIZE = 16384
RP_CHANNELS = [rp.RP_CH_1, rp.RP_CH_2, rp.RP_CH_3, rp.RP_CH_4]

METRICS_PERIOD = 60.0


class RedPitayaSatellite(DataSender):
    """Constellation Satellite to control a RedPitaya."""

    def __init__(
        self,
        *args,
        **kwargs,
    ):
        super().__init__(*args, **kwargs)
        self._readpos = 0
        self._writepos = 0

        # Define file readers for monitoring from file
        try:
            self.cpu_temperature_offset_file_reader = open(
                "/sys/devices/soc0/axi/83c00000.xadc_wiz/"
                "iio:device1/in_temp0_offset", "r")
            self.cpu_temperature_raw_file_reader = open(
                "/sys/devices/soc0/axi/83c00000.xadc_wiz/"
                "iio:device1/in_temp0_raw", "r")
            self.cpu_temperature_scale_file_reader = open(
                "/sys/devices/soc0/axi/83c00000.xadc_wiz/"
                "iio:device1/in_temp0_scale", "r")
            self.cpu_times_file_reader = open("/proc/stat", "r")
            self.memor_load_file_reader = open("/proc/meminfo", "r")
            self.network_tx_file_reader = open(
                "/sys/class/net/eth0/statistics/tx_bytes", "r")
            self.network_rx_file_reader = open(
                "/sys/class/net/eth0/statistics/rx_bytes", "r")
        except FileNotFoundError:
            self.log.warning("Failed to find path")

        self._prev_cpu_idle, self._prev_cpu_time = self._get_cpu_times()

        self._prev_tx = int(self.network_tx_file_reader.read())
        self._prev_rx = int(self.network_rx_file_reader.read())
        self.regset_readout = None
        self.active_channels = RP_CHANNELS
        self.device = None
        self.schedule_metric(
            self.get_cpu_temperature.__name__,
            self.get_cpu_temperature,
            interval=METRICS_PERIOD,
        )
        self.schedule_metric(
            self.get_cpu_load.__name__,
            self.get_cpu_load,
            interval=METRICS_PERIOD,
        )
        self.schedule_metric(
            self.get_memory_load.__name__,
            self.get_memory_load,
            interval=METRICS_PERIOD,
        )
        self.schedule_metric(
            self.get_network_rx.__name__,
            self.get_network_rx,
            interval=METRICS_PERIOD,
        )
        self.schedule_metric(
            self.get_network_tx.__name__,
            self.get_network_tx,
            interval=METRICS_PERIOD,
        )

        # Define the axi array for GPIO pins
        memory_file_handle_gpio = os.open("/dev/mem", os.O_RDWR)

        axi_mmap_gpio = mmap.mmap(
            fileno=memory_file_handle_gpio, length=mmap.PAGESIZE,
            offset=0x40000000
        )
        axi_numpy_array_gpio = np.recarray(1, axi_gpio_regset_pins,
                                           buf=axi_mmap_gpio)
        self.axi_array_contents_gpio = axi_numpy_array_gpio[0]

        rp.rp_Init()

    def do_initializing(self, payload: any) -> str:
        try:
            # Change the FPGA image ##
            bin_file = self.config["bin_file"]
            command = "/opt/redpitaya/bin/fpgautil -b " + bin_file
            if os.system(command) != 0:  # OS 2.00 and above
                msg = "System command failed."
                raise ConfigError(msg)
            time.sleep(2)

            # Define axi array for custom registers
            memory_file_handle_custom_registers = os.open("/dev/mem",
                                                          os.O_RDWR)
            axi_mmap_custom_registers = mmap.mmap(
                fileno=memory_file_handle_custom_registers,
                length=mmap.PAGESIZE, offset=self.config["offset"]
            )
            axi_numpy_array_reset = np.recarray(1, axi_regset_reset,
                                                buf=axi_mmap_custom_registers)
            self.reset_axi_array_contents = axi_numpy_array_reset[0]

            # Setting configuration values to FPGA registers
            axi_numpy_array_config = np.recarray(1, self.axi_regset_config,
                                                 buf=axi_mmap_custom_registers)
            config_axi_array_contents = axi_numpy_array_config[0]

            names = [field[0] for field in self.axi_regset_config.descr]
            for name, value in zip(names, config_axi_array_contents):
                setattr(config_axi_array_contents, name, self.config[name])

            # Define the axi array for parameters and status

            axi_numpy_array_param = np.recarray(1, self.regset_readout,
                                                buf=axi_mmap_custom_registers)
            self.axi_array_contents_param = axi_numpy_array_param[0]

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
                self.get_network_rx.__name__,
                self.get_network_rx,
                self.config["metrics_poll_rate"],
            )
            self.schedule_metric(
                self.get_network_tx.__name__,
                self.get_network_tx,
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

    def do_run(self, payload):
        """Run the satellite. Collect data from buffers and send it."""
        self.log.info("Red Pitaya satellite running, publishing events.")

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
            self.data_queue.put((payload.tobytes(), meta))

        return "Finished acquisition"

    def reset(self):
        """Reset DAQ."""
        self.reset_axi_array_contents.data_type = 16
        time.sleep(0.1)
        self.reset_axi_array_contents.data_type = self.config["data_type"]
        self._readpos = 0

    def get_data(
        self,
    ):
        """Sample every buffer channel and return raw data in numpy array."""

        # Obtain to which point the buffer has written
        self._writepos = self._get_write_pointer()

        # Skip sampling if we haven't moved
        if self._readpos == self._writepos:
            return None

        # Check if buffer has cycled
        if self._writepos < self._readpos:
            cycled = True
        else:
            cycled = False

        # Check if the amount of data is greater than 1000,
        # otherwise wait 0.1 s to not send excessive amount of packages.
        if cycled:
            if (self._writepos+BUFFER_SIZE) < (self._readpos+1000):
                time.sleep(0.1)
        else:
            if self._writepos < (self._readpos+1000):
                time.sleep(0.1)

        for i, channel in enumerate(self.active_channels):
            # Read out data buffers and adding them togheter befor returning

            if cycled:
                buffer = np.concatenate((
                    self._sample_raw32(
                        start=self._readpos,
                        stop=BUFFER_SIZE,
                        channel=channel,
                    ),
                    self._sample_raw32(
                        start=0,
                        stop=self._writepos,
                        channel=channel)))
            else:
                buffer = self._sample_raw32(
                    start=self._readpos,
                    stop=self._writepos,
                    channel=channel)

            if (i == 0):
                data = np.empty((len(self.active_channels), len(buffer)),
                                dtype=np.uint32)

            data[i] = (buffer)

        # Update readpointer
        self._readpos = self._writepos
        return data

    @cscp_requestable
    def get_device(self, _request: CSCPMessage):
        """Get name of device."""
        return (
            self.device,
            None,
            None,
        )  # NOTE: Placeholder: should be more detailed

    def get_cpu_temperature(self):
        """Obtain temperature of CPU."""

        self.cpu_temperature_offset_file_reader.seek(0)
        offset = int(self.cpu_temperature_offset_file_reader.read())
        self.cpu_temperature_raw_file_reader.seek(0)
        raw = int(self.cpu_temperature_raw_file_reader.read())
        self.cpu_temperature_scale_file_reader.seek(0)
        scale = float(self.cpu_temperature_scale_file_reader.read())
        return (round(((float)(offset + raw)) * scale / 1000.0, 1)), "C"

    def get_cpu_load(self):
        """Estimate current CPU load and update previously saved CPU times."""
        idle_cpu_time, total_cpu_time = self._get_cpu_times()
        total_cpu_time2 = total_cpu_time - self._prev_cpu_time
        idle_cpu_time2 = idle_cpu_time - self._prev_cpu_idle
        utilization = ((
            total_cpu_time2 - idle_cpu_time2) * 100) / total_cpu_time2
        self._prev_cpu_time = total_cpu_time
        self._prev_cpu_idle = idle_cpu_time
        return round(utilization, 1), "%"

    def get_memory_load(self):
        """Obtain current memory usage."""
        # Obtain memory info from file
        self.memor_load_file_reader.seek(0)
        mem = self.memor_load_file_reader.read().split("\n")
        tot_mem = int(re.search(r"\d+", mem[0]).group())
        free_mem = int(re.search(r"\d+", mem[1]).group())
        used_mem = tot_mem - free_mem

        return round(used_mem / tot_mem * 100, 1), "%"

    def get_network_tx(self):
        """Estimate current tx network speeds."""
        self.network_tx_file_reader.seek(0)
        tx_bytes = int(self.network_tx_file_reader.read())
        tx_speed = (tx_bytes - self._prev_tx) / METRICS_PERIOD
        self._prev_tx = tx_bytes
        return (round(tx_speed / 1000.0, 1)), "kb/s"

    def get_network_rx(self):
        """Estimate current rx network speeds."""
        self.network_rx_file_reader.seek(0)
        rx_bytes = int(self.network_rx_file_reader.read())
        rx_speed = (rx_bytes - self._prev_rx) / METRICS_PERIOD
        self._prev_rx = rx_bytes

        return (round(rx_speed / 1000, 1)), "kb/s"

    def get_digital_gpio_pins(self):
        """Read out values at digital gpio P and N ports."""
        p_pins = self.axi_array_contents_gpio.p_pins.astype(
            dtype=np.uint8).item()
        n_pins = self.axi_array_contents_gpio.n_pins.astype(
            dtype=np.uint8).item()

        pins = [p_pins, n_pins]
        return pins, "bits"

    def get_analog_gpio_pins(self):
        """Read out values at analog gpio ports."""
        pins = []
        for pin in range(4):
            pins.append(rp.rp_AIpinGetValue(pin)[1])
        return pins, "bits"

    def _get_write_pointer(self):
        """Obtain write pointer"""
        return rp.rp_AcqGetWritePointer()[1]

    def read_registers(self):
        """
        Reads the stored values of the axi_regset and returns a
        tuple of their respective names and values.

        If no readout of axi_regset is specified the method returns None.
        """
        if not self.regset_readout:
            return None

        names = [field[0] for field in self.regset_readout.descr]

        ret = {}
        for name, value in zip(names, self.axi_array_contents_param):
            ret[name] = value.item()
        return ret, "uint32"

    def _sample_raw(self, channel, buffer, chunk):
        """Sample data from given channel."""

        rp.rp_AcqGetDataRaw(channel, self._readpos, chunk, buffer.cast())

        data_raw = np.zeros(chunk, dtype=int)

        for idx in range(0, chunk, 1):
            data_raw[idx] = buffer[idx]

        return data_raw

    def _sample_raw32(self, start: int = 0, stop: int = 16384,
                      channel: int = 1):
        """Read out data in 32 bit form."""

        class Array(ctypes.Structure):
            """Define the struct in Python"""

            _fields_ = [("data", ctypes.POINTER(ctypes.c_uint32))]

        # Load the shared library.
        # NOTE: I don't think this path will work well when packaging
        # NOTE: This might have some answers when the time comes:
        # https://stackoverflow.com/questions/51468432/refer-to-a-file-within-python-package
        lib = ctypes.CDLL("python/constellation/satellites/read_data32bit.so")

        # Define the argument and return types of the function
        lib.readData.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
        lib.readData.restype = Array

        # Define register offset depending onc channel
        if channel == rp.RP_CH_1:
            offset = 0x40110000
        elif channel == rp.RP_CH_2:
            offset = 0x40120000
        elif channel == rp.RP_CH_3:
            offset = 0x40210000
        elif channel == rp.RP_CH_4:
            offset = 0x40220000

        # Call the C function
        result = lib.readData(0, 16384, offset)

        # Convert the result to a NumPy array

        data_array = np.ctypeslib.as_array(result.data,
                                           shape=(16384,))[start:stop]
        stored_data = data_array.copy()
        lib.freeData(result.data)
        return stored_data

    def _get_cpu_times(self):
        """Obtain idle time and active time of CPU."""
        # Get the line containing total values of CPU time
        self.cpu_times_file_reader.seek(0)
        stat = self.cpu_times_file_reader.read().split("\n")[0].split(" ")[2:]

        idle_cpu_time = 0
        total_cpu_time = 0
        for idx, val in enumerate(stat):
            total_cpu_time += int(val)
            if idx == 3:
                idle_cpu_time = int(val)

        return idle_cpu_time, total_cpu_time

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
