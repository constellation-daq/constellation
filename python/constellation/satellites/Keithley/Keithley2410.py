"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

Serial interface for a Keithley Series 2400 SourceMeter as voltage source.

Keithley needs to be set to RS-232 mode with:
- BAUD: 19200
- BITS: 8
- PARITY: EVEN
- TERMINATOR: <CR+LF>
- FLOW_CONTROL: NONE
"""

import threading

import serial

from .KeithleyInterface import KeithleyInterface


class Keithley2410(KeithleyInterface):
    def __init__(
        self,
        port: str,
    ):
        super().__init__(
            port=port,
            baud=19200,
            bits=serial.EIGHTBITS,
            stopbits=serial.STOPBITS_ONE,
            parity=serial.PARITY_EVEN,
            terminator="\r\n",
            flow_ctrl_xon_xoff=False,
        )
        self._output_lock = threading.Lock()

    # Device functions

    def reset(self):
        self._write("*RST")

    def identify(self) -> str:
        return self._write_read("*IDN?")

    def enable_output(self, enable: bool):
        with self._output_lock:
            on_off = "ON" if enable else "OFF"
            self._write(f":OUTP {on_off}")

    def output_enabled(self) -> bool:
        ret = self._write_read(":OUTP?")
        return ret != "0"

    def get_terminals(self) -> list[str]:
        return ["front", "rear"]

    def set_terminal(self, terminal: str):
        if terminal.lower() not in ["front", "rear"]:
            raise ValueError("Only front and rear terminal supported")
        self._write(f":ROUT:TERM {terminal[:4].upper()}")

    def get_terminal(self) -> str:
        terminal = self._write_read(":ROUT:TERM?").lower()
        if terminal == "fron":  # codespell:ignore fron
            terminal += "t"
        return terminal

    def set_voltage(self, voltage: float):
        self._write(f":SOUR:VOLT:LEV {voltage}")

    def get_voltage(self) -> float:
        return float(self._write_read(":SOUR:VOLT:LEV?"))

    def set_ovp(self, voltage: float):
        self._write(f":SOUR:VOLT:PROT:LEV {voltage}")

    def get_ovp(self) -> float:
        return float(self._write_read(":SOUR:VOLT:PROT:LEV?"))

    def set_compliance(self, current: float):
        self._write(f":SENS:CURR:PROT:LEV {current}")

    def get_compliance(self) -> float:
        return float(self._write_read(":SENS:CURR:PROT:LEV?"))

    def in_compliance(self) -> bool:
        with self._output_lock:
            if self.output_enabled():
                tripped = self._write_read(":SENS:CURR:PROT:TRIP?")
                # Returns "0" for no, "1" for yes
                return tripped != "0"
        return False

    def read_output(self) -> tuple[float, float, float]:
        with self._output_lock:
            if self.output_enabled():
                voltage, current, timestamp = self._write_read(":READ?").split(",")
                return float(voltage), float(current), float(timestamp)
        return 0.0, 0.0, 0.0

    # Device helper functions

    def initialize(self):
        self.reset()
        # Set data format to ascii (comma-separated)
        self._write(":FORM:DATA ASC")
        # Output voltage, current and timestamp
        self._write(":FORM:ELEM VOLT, CURR, TIME")
        # Set buffer to one reading
        self._write(":TRAC:POIN 1")  # codespell:ignore poin
        # Set trigger to take one reading
        self._write(":TRIG:COUN 1")

    def release(self):
        self._write(":SYST:LOC")
