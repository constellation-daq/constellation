"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides the classes for communication with Mikroe Expand11 Module
"""

import smbus2


class Expand11:
    # Constants
    EXPAND11_REG_INPUT_PORT = 0x00
    EXPAND11_REG_OUTPUT_PORT = 0x01
    EXPAND11_REG_POLARITY_INV = 0x02
    EXPAND11_REG_CONFIG = 0x03
    EXPAND11_REG_SPECIAL_FUNC = 0x50

    EXPAND11_ALL_PINS_MASK = 0xFF
    EXPAND11_NO_PIN_MASK = 0x00

    EXPAND11_OUTPUT_DIRECTION = 0x00
    EXPAND11_INPUT_DIRECTION = 0xFF

    EXPAND11_POLARITY_NO_INV = 0x00
    EXPAND11_POLARITY_INV = 0x01

    EXPAND11_ERROR = -1
    EXPAND11_OK = 0

    EXPAND11_PIN_0_MASK = 0x01
    EXPAND11_PIN_1_MASK = 0x02
    EXPAND11_PIN_2_MASK = 0x04
    EXPAND11_PIN_3_MASK = 0x08

    EXPAND11_SPECIAL_FUNC_P3_AS_INT = 0x80
    EXPAND11_SPECIAL_FUNC_PU_DISABLED = 0x40
    EXPAND11_P3_AS_P3 = 0x00
    EXPAND11_P3_AS_INT = 0x01

    def __init__(self, i2c_bus, i2c_address):
        self.i2c = smbus2.SMBus(i2c_bus)
        self.address = i2c_address

    def write_register(self, reg, data):
        """Write a byte to a register."""
        try:
            self.i2c.write_byte_data(self.address, reg, data)
            return self.EXPAND11_OK
        except FileNotFoundError:
            return self.EXPAND11_ERROR

    def read_register(self, reg):
        """Read a byte from a register."""
        try:
            return self.i2c.read_byte_data(self.address, reg)
        except FileNotFoundError:
            return self.EXPAND11_ERROR

    def set_pin_direction(self, direction, pin_mask):
        """Set the direction of specific pins."""
        if (
            pin_mask > self.EXPAND11_ALL_PINS_MASK
            or direction > self.EXPAND11_INPUT_DIRECTION
        ):
            return self.EXPAND11_ERROR

        config = self.read_register(self.EXPAND11_REG_CONFIG)
        if config == self.EXPAND11_ERROR:
            return self.EXPAND11_ERROR

        if direction == self.EXPAND11_OUTPUT_DIRECTION:
            config &= ~pin_mask
        else:
            config |= pin_mask

        return self.write_register(self.EXPAND11_REG_CONFIG, config)

    def default_cfg(self):
        # Implement the default configuration,
        try:
            self.write_register(
                self.EXPAND11_REG_CONFIG, self.EXPAND11_ALL_PINS_MASK
            )  # Setting all pins to input
            self.set_pin_polarity(
                self.EXPAND11_POLARITY_INV, self.EXPAND11_ALL_PINS_MASK
            )  # Setting inverted polarity
            self.enable_pull_up()  # Activating Pull-up
            return self.EXPAND11_OK
        except FileNotFoundError:

            return self.EXPAND11_ERROR

    def set_all_pins_direction(self, direction):
        """Set the direction of all pins."""
        if direction > self.EXPAND11_INPUT_DIRECTION:
            return self.EXPAND11_ERROR

        if direction == self.EXPAND11_OUTPUT_DIRECTION:
            return self.write_register(
                self.EXPAND11_REG_CONFIG, self.EXPAND11_NO_PIN_MASK
            )
        else:
            return self.write_register(
                self.EXPAND11_REG_CONFIG, self.EXPAND11_ALL_PINS_MASK
            )

    def set_pin_polarity(self, polarity, pin_mask):
        """Set the polarity of specific pins."""
        if (
            pin_mask > self.EXPAND11_ALL_PINS_MASK
            or polarity > self.EXPAND11_POLARITY_INV
        ):
            return self.EXPAND11_ERROR

        config = self.read_register(self.EXPAND11_REG_POLARITY_INV)
        if config == self.EXPAND11_ERROR:
            return self.EXPAND11_ERROR

        if polarity == self.EXPAND11_POLARITY_NO_INV:
            config &= ~pin_mask
        else:
            config |= pin_mask

        return self.write_register(self.EXPAND11_REG_POLARITY_INV, config)

    def write_port_value(self, value):
        """Write a value to the output port."""
        return self.write_register(self.EXPAND11_REG_OUTPUT_PORT, value)

    def read_port_value(self):
        """Read the value of the input port."""
        try:
            port_value = self.read_register(self.EXPAND11_REG_INPUT_PORT)
        except FileNotFoundError:
            return [-1, -1, -1, -1]
        return [
            int((port_value & Expand11.EXPAND11_PIN_0_MASK) > 0),
            int((port_value & Expand11.EXPAND11_PIN_1_MASK) > 0),
            int((port_value & Expand11.EXPAND11_PIN_2_MASK) > 0),
            int((port_value & Expand11.EXPAND11_PIN_3_MASK) > 0),
        ]

    def set_p3_function(self, p3_func):
        """Set the P3 pin function to P3 or INT."""
        if p3_func > 1:
            return self.EXPAND11_ERROR

        config = self.read_register(self.EXPAND11_REG_SPECIAL_FUNC)
        if config == self.EXPAND11_ERROR:
            return self.EXPAND11_ERROR

        if p3_func:
            config |= (
                self.EXPAND11_SPECIAL_FUNC_P3_AS_INT
            )  # Assuming 3rd bit for P3_FUNC_INT
        else:
            config &= ~self.EXPAND11_SPECIAL_FUNC_P3_AS_INT  # Clear the 3rd bit

        return self.write_register(self.EXPAND11_REG_SPECIAL_FUNC, config)

    def disable_pull_up(self):
        """Disable internal pull-ups for input pins."""
        config = self.read_register(self.EXPAND11_REG_SPECIAL_FUNC)
        if config == self.EXPAND11_ERROR:
            return self.EXPAND11_ERROR
        print(config)
        config |= (
            self.EXPAND11_SPECIAL_FUNC_PU_DISABLED
        )  # Assuming 4th bit for PULL_UP_DISABLE
        print(config)
        return self.write_register(self.EXPAND11_REG_SPECIAL_FUNC, config)

    def enable_pull_up(self):
        """Enable internal pull-ups for input pins."""
        config = self.read_register(self.EXPAND11_REG_SPECIAL_FUNC)
        if config == self.EXPAND11_ERROR:
            return self.EXPAND11_ERROR

        config &= ~self.EXPAND11_SPECIAL_FUNC_PU_DISABLED
        # Assuming 4th bit for PULL_UP_ENABLE
        print(config)
        return self.write_register(self.EXPAND11_REG_SPECIAL_FUNC, config)
