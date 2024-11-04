"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

Abstract serial interface for Keithleys.
"""

from abc import ABCMeta, abstractmethod
import time
from threading import Lock

import serial


class KeithleyInterface(metaclass=ABCMeta):
    def __init__(
        self,
        port: str,
        baud: int,
        bits: int,
        stopbits: int,
        parity: str,
        terminator: str,
        flow_ctrl_xon_xoff: bool,
        timeout: float = 2,
    ):
        self._serial = serial.Serial(
            port=port,
            baudrate=baud,
            bytesize=bits,
            stopbits=stopbits,
            parity=parity,
            xonxoff=flow_ctrl_xon_xoff,
            timeout=timeout,
        )
        self._terminator = terminator
        self._lock = Lock()

    # Serial helper functions

    def _write(self, command: str):
        """
        Write command to serial port
        """
        with self._lock:
            self._serial.write((command + self._terminator).encode())

    def _write_read(self, command: str) -> str:
        """
        Write command to serial and then read until terminator or timeout
        """
        with self._lock:
            self._serial.write((command + self._terminator).encode())
            return self._serial.read_until(self._terminator.encode()).decode().strip(self._terminator)

    # Device functions

    @abstractmethod
    def reset(self):
        """
        Resets device settings
        """
        pass

    @abstractmethod
    def identify(self) -> str:
        """
        Identifies device
        """
        pass

    @abstractmethod
    def enable_output(self, enable: bool):
        """
        Enable or disable output
        """
        pass

    @abstractmethod
    def output_enabled(self) -> bool:
        """
        If the output is enabled
        """
        pass

    @abstractmethod
    def get_terminals(self) -> list[str]:
        """
        List of terminals which can be controlled
        """
        pass

    @abstractmethod
    def set_terminal(self, terminal: str):
        """
        Terminal for which to control output
        """
        pass

    @abstractmethod
    def get_terminal(self) -> str:
        """
        Get terminal for which output is controlled
        """
        pass

    @abstractmethod
    def set_voltage(self, voltage: float):
        """
        Set output voltage
        """
        pass

    @abstractmethod
    def get_voltage(self) -> float:
        """
        Get output voltage
        """
        pass

    @abstractmethod
    def set_ovp(self, voltage: float):
        """
        Set over-voltage protection voltage
        """
        pass

    @abstractmethod
    def get_ovp(self) -> float:
        """
        Get over-voltage protection voltage
        """
        pass

    @abstractmethod
    def set_compliance(self, current: float):
        """
        Set current compliance
        """
        pass

    @abstractmethod
    def get_compliance(self) -> float:
        """
        Get current compliance
        """
        pass

    @abstractmethod
    def in_compliance(self) -> bool:
        """
        If current is in compliance
        """
        pass

    @abstractmethod
    def read_output(self) -> tuple[float, float, float]:
        """
        Reads voltage, current, timestamp
        """
        pass

    # Device helper functions

    @abstractmethod
    def initialize(self):
        """
        Resets device and initializes appropriate settings
        """
        pass

    @abstractmethod
    def release(self):
        """
        Release device from remote mode
        """
        pass

    def ramp_voltage(self, voltage_target: float, voltage_step: float, settle_time: float):
        """ """
        if voltage_step <= 0:
            raise ValueError("voltage step needs to be positive")
        if settle_time <= 0:
            raise ValueError("settle time needs to be positive")

        voltage_current = self.get_voltage()
        ramp_up = voltage_target > voltage_current

        # Lambda to evaluate if another step should be added
        do_next_step = lambda voltage: voltage + voltage_step < voltage_target
        if not ramp_up:
            voltage_step *= -1
            do_next_step = lambda voltage: voltage + voltage_step > voltage_target

        while voltage_current != voltage_target:
            # Check if another step can be added without exceeding target
            if do_next_step(voltage_current):
                voltage_current += voltage_step
            else:
                voltage_current = voltage_target

            # Set voltage and settle
            self.set_voltage(voltage_current)
            time.sleep(settle_time)
