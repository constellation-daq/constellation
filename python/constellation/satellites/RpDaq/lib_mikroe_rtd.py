"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides the classes for communication with Mikroe RTD Module
"""

import spidev
import rp
import time


class RTD:
    """Class for for communication with Mikroe RTD Module"""

    def __init__(self, spi_speed=1000000, spi_mode=0b01, microbus=1):
        """Inits the communication"""
        self.spi = spidev.SpiDev()
        self.spi.open(1, 0)
        self.spi.max_speed_hz = spi_speed
        self.spi.mode = spi_mode
        self.spi.cshigh = True
        self.spi.bits_per_word = 8

        if microbus == 2:
            self.cs = rp.RP_DIO1_N
            self.drdy = rp.RP_DIO2_P
        else:
            self.cs = rp.RP_DIO3_N
            self.drdy = rp.RP_DIO4_P
        rp.rp_DpinSetDirection(self.cs, rp.RP_OUT)
        rp.rp_DpinSetDirection(self.drdy, rp.RP_IN)
        rp.rp_DpinSetState(self.cs, rp.RP_HIGH)

    def default_cfg(self):
        """Configuring unit"""
        self.write_register(0x00, 0x83)
        time.sleep(0.1)
        self.write_register(0x00, 0xC2)
        tmp_nr = self.read_register(0x00)
        if tmp_nr == 255:
            return -1
        else:
            return 0

    def write_register(self, reg_address, write_data):
        """Write data to the register"""
        # Prepare the data to be sent
        reg_address |= 0x80
        data = [reg_address, write_data]

        # Write data over SPI
        time.sleep(0.1)
        rp.rp_DpinSetState(self.cs, rp.RP_LOW)
        time.sleep(0.1)
        self.spi.xfer2(data)
        time.sleep(0.1)
        rp.rp_DpinSetState(self.cs, rp.RP_HIGH)

    def read_register(self, reg_address):
        """Read data from register"""
        # Read data from a register over SPI
        time.sleep(0.1)
        rp.rp_DpinSetState(self.cs, rp.RP_LOW)
        time.sleep(0.1)
        self.spi.xfer2([reg_address])
        time.sleep(0.1)
        result = self.spi.xfer2([0x00])
        time.sleep(0.1)
        rp.rp_DpinSetState(self.cs, rp.RP_HIGH)
        return result[0]

    def get_rtd_temperature(self):
        # Read raw temperature data from the sensor

        temp_msb = self.read_register(0x01)
        temp_msb = temp_msb << 8

        temp_lsb = self.read_register(0x02)

        raw_temp = temp_msb | temp_lsb
        raw_temp = raw_temp
        return self.convert_temperature(raw_temp, 470)

    def convert_temperature(self, input_data, referent_resistance):
        """Convert raw temperature data to Celsius"""
        coefficient = referent_resistance / 400.0
        input_data >>= 1
        temperature = (input_data * coefficient) / 32 - 256
        return round(temperature, 2)

    def close(self):
        """Closes the connection and clean up"""
        self.spi.close()
        rp.rp_DpinSetState(self.cs, rp.RP_LOW)
        rp.rp_DpinSetDirection(self.cs, rp.RP_IN)
        rp.rp_DpinSetDirection(self.drdy, rp.RP_IN)
