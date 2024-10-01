#!/usr/bin/env python3

"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides an implementation for a Constellation Satellite on a
RedPitaya device.
"""

import mmap
import os
import time

import re
import numpy as np
import rp

from .lib_mikroe_expand11 import Expand11
from .lib_mikroe_rtd import RTD

from constellation.core.commandmanager import cscp_requestable
from constellation.core.cscp import CSCPMessage
from constellation.core.datasender import DataSender
from constellation.core.configuration import ConfigError
from constellation.core.satellite import SatelliteArgumentParser
from constellation.core.base import setup_cli_logging

axi_regset_start_stop = np.dtype([("Externaltrigger", "uint32")])

axi_regset_data_type = np.dtype([("data_type", "uint32")])

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
        self.sample_time = 0
        self.data_type = 0
        self.memory_file_handle = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
        rp.rp_Init()
        # Define file readers for monitoring from file
        try:

            self.cpu_temperature_offset_file_reader = open(
                "/sys/devices/soc0/axi/83c00000.xadc_wiz/" "iio:device1/in_temp0_offset",
                "r",
            )
            self.cpu_temperature_raw_file_reader = open(
                "/sys/devices/soc0/axi/83c00000.xadc_wiz/" "iio:device1/in_temp0_raw",
                "r",
            )
            self.cpu_temperature_scale_file_reader = open(
                "/sys/devices/soc0/axi/83c00000.xadc_wiz/" "iio:device1/in_temp0_scale",
                "r",
            )
            self.cpu_times_file_reader = open("/proc/stat", "r")
            self.memor_load_file_reader = open("/proc/meminfo", "r")
            self.network_tx_file_reader = open("/sys/class/net/eth0/statistics/tx_bytes", "r")
            self.network_rx_file_reader = open("/sys/class/net/eth0/statistics/rx_bytes", "r")
        except FileNotFoundError:
            self.log.warning("Failed to find path of monitoring files")

        self._prev_cpu_idle, self._prev_cpu_time = self._get_cpu_times()

        self._prev_tx = int(self.network_tx_file_reader.read())
        self._prev_rx = int(self.network_rx_file_reader.read())
        self.regset_readout = None
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

    def do_reconfigure(self, payload: any) -> str:
        # Writes FPGA configurations to register
        time.sleep(1)
        names = [field[0] for field in self.axi_regset_config.descr]
        for name, value in zip(names, self.config_axi_array_contents):
            setattr(self.config_axi_array_contents, name, self.config[name])
        return "Reconfigured"

    def do_initializing(self, payload: any) -> str:
        try:
            # Change the FPGA image ##
            bin_file = self.config["bin_file"]
            command = "/opt/redpitaya/bin/fpgautil -b " + bin_file
            if os.system(command) != 0:  # OS 2.00 and above
                msg = "System command failed."
                raise ConfigError(msg)
            time.sleep(3)

            # Define axi array for custom registers
            axi_mmap_custom_registers = mmap.mmap(
                self.memory_file_handle,
                mmap.PAGESIZE,
                mmap.MAP_SHARED,
                mmap.PROT_READ | mmap.PROT_WRITE,
                offset=0x40600000,
            )
            axi_numpy_array_reset = np.recarray(1, axi_regset_data_type, buf=axi_mmap_custom_registers)
            self.data_type_axi_array_contents = axi_numpy_array_reset[0]

            # Setting configuration values to FPGA registers
            axi_numpy_array_config = np.recarray(1, self.axi_regset_config, buf=axi_mmap_custom_registers)
            self.config_axi_array_contents = axi_numpy_array_config[0]

            # Define the axi array for parameters and status
            axi_numpy_array_param = np.recarray(1, self.regset_readout, buf=axi_mmap_custom_registers)
            self.axi_array_contents_param = axi_numpy_array_param[0]
            time.sleep(0.1)
            # Writes FPGA configurations to register
            names = [field[0] for field in self.axi_regset_config.descr]
            for name, value in zip(names, self.config_axi_array_contents):
                setattr(self.config_axi_array_contents, name, self.config[name])

            # Define the axi array for axi writer channel 1 2
            self.axi_writer_register_names = np.dtype(
                [
                    ("not_used1", "uint32"),
                    ("not_used2", "uint32"),
                    ("not_used3", "uint32"),
                    ("not_used4", "uint32"),
                    ("not_used5", "uint32"),
                    ("not_used6", "uint32"),
                    ("not_used7", "uint32"),
                    ("not_used8", "uint32"),
                    ("not_used9", "uint32"),
                    ("not_used10", "uint32"),
                    ("not_used11", "uint32"),
                    ("not_used12", "uint32"),
                    ("not_used13", "uint32"),
                    ("not_used14", "uint32"),
                    ("not_used15", "uint32"),
                    ("not_used16", "uint32"),
                    ("not_used17", "uint32"),
                    ("not_used18", "uint32"),
                    ("not_used19", "uint32"),
                    ("not_used20", "uint32"),
                    ("lower_address_0", "uint32"),
                    ("upper_address_0", "uint32"),
                    ("not_used21", "uint32"),
                    ("enable_master_0", "uint32"),
                    ("current_trigger_pointer", "uint32"),
                    ("current_write_pointer", "uint32"),
                    ("not_used24", "uint32"),
                    ("not_used25", "uint32"),
                    ("lower_address_1", "uint32"),
                    ("upper_address_1", "uint32"),
                    ("not_used26", "uint32"),
                    ("enable_master_1", "uint32"),
                ]
            )

            axi_writer_mmap0 = mmap.mmap(
                self.memory_file_handle,
                mmap.PAGESIZE,
                mmap.MAP_SHARED,
                mmap.PROT_READ | mmap.PROT_WRITE,
                offset=0x40100000,
            )
            axi_writer_numpy_array0 = np.recarray(1, self.axi_writer_register_names, buf=axi_writer_mmap0)
            time.sleep(0.1)
            self.axi_writer_contents0 = axi_writer_numpy_array0[0]
            self.axi_writer_contents0.lower_address_0 = 0x1000000
            self.axi_writer_contents0.upper_address_0 = 0x107FFF8
            self.axi_writer_contents0.enable_master_0 = 1
            self.axi_writer_contents0.lower_address_1 = 0x1080000
            self.axi_writer_contents0.upper_address_1 = 0x10FFFF8
            self.axi_writer_contents0.enable_master_1 = 1

            if len(self.active_channels) == 4:
                # Define the axi array for axi writer channel 3 4
                axi_writer_mmap2 = mmap.mmap(
                    self.memory_file_handle,
                    mmap.PAGESIZE,
                    mmap.MAP_SHARED,
                    mmap.PROT_READ | mmap.PROT_WRITE,
                    offset=0x40200000,
                )
                axi_writer_numpy_array2 = np.recarray(1, self.axi_writer_register_names, buf=axi_writer_mmap2)
                axi_writer_contents2 = axi_writer_numpy_array2[0]
                axi_writer_contents2.lower_address_0 = 0x1100000
                axi_writer_contents2.upper_address_0 = 0x117FFF8
                axi_writer_contents2.enable_master_0 = 1
                axi_writer_contents2.lower_address_1 = 0x1180000
                axi_writer_contents2.upper_address_1 = 0x11FFFF8
                axi_writer_contents2.enable_master_1 = 1

            # # Define memory map for reading data
            self.read_data_axi_mmap = mmap.mmap(
                self.memory_file_handle,
                0x200000,
                mmap.MAP_SHARED,
                mmap.PROT_READ | mmap.PROT_WRITE,
                offset=0x1000000,
            )
            time.sleep(0.1)
            # Resetting ADC
            self.reset()

            # Setup metrics
            if self.config["read_water_sensors"]:
                self.expand11 = Expand11(i2c_bus=0, i2c_address=0x41)

                if self.expand11.default_cfg() != -1:
                    self.schedule_metric(
                        self.get_water_sensor_state.__name__,
                        self.get_water_sensor_state,
                        self.config["water_sensors_poll_rate"],
                    )
                else:
                    self.log.warning("Could not connect to Expand11 card")

            if self.config["read_temperature_sensors"]:
                self.rtd = RTD(microbus=1)
                if self.rtd.default_cfg() != -1:
                    self.schedule_metric(
                        self.rtd.get_rtd_temperature.__name__,
                        self.rtd.get_rtd_temperature,
                        self.config["temperature_sensors_poll_rate"],
                    )
                else:
                    self.log.warning("Could not connect to RTD card")

            if self.config["read_gpio"]:
                # Define the axi array for GPIO pins

                axi_mmap_gpio = mmap.mmap(
                    self.memory_file_handle,
                    mmap.PAGESIZE,
                    mmap.MAP_SHARED,
                    mmap.PROT_READ | mmap.PROT_WRITE,
                    offset=0x40000000,
                )
                axi_numpy_array_gpio = np.recarray(1, axi_gpio_regset_pins, buf=axi_mmap_gpio)
                self.axi_array_contents_gpio = axi_numpy_array_gpio[0]

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
        return "Initialized."

    def get_water_sensor_state(self):

        names = [
            "water_sensor_ch0",
            "water_sensor_ch1",
            "water_sensor_ch2",
            "water_sensor_ch3",
        ]
        values = self.expand11.read_port_value()
        ret = {}
        for name, value in zip(names, values):
            ret[name] = value
        return ret, "uint8"

        return self.expand11.read_port_value()

    def do_starting(self, payload):
        """Starting the acquisition and Wrote BOR"""
        # tmp_BOR = self.config._config
        # tmp_BOR["start time"] = time.strftime("%Y-%m-%d-%H%M%S", time.localtime())
        # self.BOR = tmp_BOR
        self.data_type = self.config["data_type"]
        return "Started"

    def do_run(self, payload):
        """Run the satellite. Collect data from buffers and send it."""
        self.log.info("Red Pitaya satellite running, publishing events.")

        while not self._state_thread_evt.is_set():
            # Main DAQ-loop
            payload = self.get_axi_data()

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
        self.data_type_axi_array_contents.data_type = self.config["data_type"] | (1 << 4)
        time.sleep(0.1)
        self.data_type_axi_array_contents.data_type = self.config["data_type"]
        self._readpos = 0

    def do_stopping(self, payload: any):
        """Stop acquisition and read out last buffer"""
        tmp_EOR = self.read_registers()[0]
        tmp_EOR["stop time"] = time.strftime("%Y-%m-%d-%H%M%S", time.localtime())
        self.EOR = tmp_EOR

        time.sleep(1)  # add sleep to make sure that everything has stopped

        # Read last buffer
        self.sample_time = self.sample_time - 10
        payload = self.get_axi_data()

        if payload is not None:

            # Include data type as part of meta
            meta = {
                "dtype": f"{payload.dtype}",
            }

            # Format payload to serializable
            self.data_queue.put((payload.tobytes(), meta))
        self.reset()
        return "Stopped RedPitaya Satellite."

    def get_axi_data(
        self,
    ):
        BUFFER_SIZE = 131072
        """Sample every buffer channel and return raw data in numpy array."""

        if (self.data_type >> 0) & 1 == 1:
            if (self.data_type_axi_array_contents.data_type >> 7) & 1 == 0:
                time.sleep(0.01)
                return None
            else:
                time.sleep(0.01)
        self._writepos = self._get_axi_write_pointer()

        # Skip sampling if we haven't moved
        if self._readpos == self._writepos:
            return None

        # Check if buffer has cycled
        if self._writepos < self._readpos:
            cycled = True
        else:
            cycled = False

        # Check if the amount of data is greater than 10000 if 2 second has passed since last read,
        # otherwise wait 0.01 s and retry. This to not send excessive amount of packages.
        if cycled:
            if (self._writepos + BUFFER_SIZE) < (self._readpos + 10000) and time.time() - self.sample_time < 2:
                time.sleep(0.01)
                return None
        else:
            if self._writepos < (self._readpos + 10000) and time.time() - self.sample_time < 2:
                time.sleep(0.01)
                return None

        for i, channel in enumerate(self.active_channels):
            # Read out data buffers and adding them together before returning

            if cycled:
                buffer = np.concatenate(
                    (
                        self._sample_axi_raw32(
                            start=self._readpos,
                            stop=BUFFER_SIZE,
                            channel=channel,
                        ),
                        self._sample_axi_raw32(start=0, stop=self._writepos, channel=channel),
                    )
                )
            else:
                buffer = self._sample_axi_raw32(start=self._readpos, stop=self._writepos, channel=channel)

            if i == 0:
                data = np.empty((len(self.active_channels), len(buffer)), dtype=np.uint32)

            data[i] = buffer

        if (self.data_type >> 0) & 1 == 1:
            self.data_type_axi_array_contents.data_type = self.data_type | (1 << 5) | (1 << 6)
            time.sleep(0.01)
            self.data_type_axi_array_contents.data_type = self.data_type | (1 << 5)

        # Update read pointer and sample time
        self._readpos = self._writepos
        self.sample_time = time.time()
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
        return (round(((float)(offset + raw)) * scale / 1000.0, 1)), "°C"

    def get_cpu_load(self):
        """Estimate current CPU load and update previously saved CPU times."""
        idle_cpu_time, total_cpu_time = self._get_cpu_times()
        total_cpu_time2 = total_cpu_time - self._prev_cpu_time
        idle_cpu_time2 = idle_cpu_time - self._prev_cpu_idle
        utilization = ((total_cpu_time2 - idle_cpu_time2) * 100) / total_cpu_time2
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
        p_pins = self.axi_array_contents_gpio.p_pins.astype(dtype=np.uint8).item()
        n_pins = self.axi_array_contents_gpio.n_pins.astype(dtype=np.uint8).item()

        pins = [p_pins, n_pins]
        return pins, "bits"

    def get_analog_gpio_pins(self):
        """Read out values at analog gpio ports."""
        pins = []
        for pin in range(4):
            pins.append(rp.rp_AIpinGetValue(pin)[1])
        return pins, "bits"

    def _get_axi_write_pointer(self):
        """Obtain _axi_write pointer"""

        write_pointer = int((self.axi_writer_contents0.current_write_pointer - 0x1000000) / 4)
        if write_pointer < 0 or 131072 < write_pointer:
            # To take care of negative number in initialization
            write_pointer = 0
        return write_pointer

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

    def _sample_axi_raw32(self, start: int = 0, stop: int = 16384, channel: int = 1):
        """Read out data in 32-bit form."""

        def read_data(start_offset, stop_offset):
            start = start_offset // 4
            stop = stop_offset // 4
            chunk_length = stop - start

            # Directly map the memory region as a NumPy array
            data_out = np.frombuffer(
                self.read_data_axi_mmap,
                dtype=np.uint32,
                count=chunk_length,
                offset=start * 4,
            )

            return data_out

        # Define register offset depending on channel
        if channel == rp.RP_CH_1:
            result = read_data(4 * start, 4 * stop)
        elif channel == rp.RP_CH_2:
            result = read_data(4 * start + 0x080000, 4 * stop + 0x080000)
        elif channel == rp.RP_CH_3:
            result = read_data(4 * start + 0x100000, 4 * stop + 0x100000)
        elif channel == rp.RP_CH_4:
            result = read_data(4 * start + 0x180000, 4 * stop + 0x180000)

        return result

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
    parser = SatelliteArgumentParser(
        description=main.__doc__,
        epilog="This is a 3rd-party component of Constellation.",
    )
    # this sets the defaults for our Satellite
    parser.set_defaults(
        name="RedPitaya_data_sender",
        cmd_port=23999,
        mon_port=55556,
        hb_port=61234,
        data_port=55557,
    )
    args = vars(parser.parse_args(args))

    # set up logging
    setup_cli_logging(args["name"], args.pop("log_level"))

    # start server with remaining args
    s = RedPitayaSatellite(**args)
    s.run_satellite()


if __name__ == "__main__":
    main()
