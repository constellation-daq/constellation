"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import threading

import serial

from .KeithleyInterface import KeithleyInterface


class Keithley6517(KeithleyInterface):
    def __init__(
        self,
        port: str,
    ):
        super().__init__(
            port=port,
            baud=19200,
            bits=serial.EIGHTBITS,
            stopbits=serial.STOPBITS_TWO,
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
            self._write(f":OUTPUT {on_off}")

    def output_enabled(self) -> bool:
        ret = self._write_read(":OUTPUT?")
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
        self._write(":FORM:ELEM VSO, READ, TST")
        # Set buffer to one reading
        self._write(":TRAC:POIN 1")  # codespell:ignore poin
        # Set trigger to take one reading
        self._write(":TRIG:COUN 1")

    def release(self):
        self._write(":SYST:LOC")

    # ===========================================================================
    # Do initial configuration
    # ===========================================================================
    def set_device_configuration(self):
        # Initialization of the Serial interface
        try:
            # Set up the source

            self._ser.write(("*rst" + "\r\n").encode("utf-8"))
            # self._ser.write((':SYST:PRESet' + "\r\n").encode('utf-8'))
            self._ser.write((":SYST:ZCH OFF" + "\r\n").encode("utf-8"))
            self._ser.write((":CALC:FORM NONE" + "\r\n").encode("utf-8"))

            self._ser.write((":SOUR:VOLT:LIM " + str(self._OVPSource) + "\r\n").encode("utf-8"))

            # Set up the sensing. Can be voltage, current, or resistance
            self._ser.write((':SENS:FUNC "' + self._measure + '"\r\n').encode("utf-8"))
            self._ser.write(
                (":SENS:" + self._measure + ":RANG:AUTO " + str(self._autorangeMeasure) + "\r\n").encode("utf-8")
            )

            # Set up the buffer
            self._ser.write(b":TRAC:FEED:CONT NEVer\r\n")  # Disable buffer storage

            self._ser.write(b":TRAC:CLEar\r\n")  # Clears the buffer
            self._ser.write(b":TRAC:FEED:CONT NEXT\r\n")  # Enable buffer storage. Fills the buffer, then stops

            self._ser.write(str.encode(":TRIG:DELay " + str(self._triggerDelay) + "\r\n"))

        except ValueError:
            print("ERROR: No serial connection. Check cable and port!")

    def set_source_upper_range(self, senseUpperRange):
        self._ser.write(str.encode(":SENSE:VOLT:RANG:UPP " + senseUpperRange + "\r\n"))

    # Read from the buffer
    def sample(self, no_of_samples):
        self._ser.write(b":TRAC:FEED:CONT NEVer\r\n")  # Disable buffer storage
        self._ser.write(b":TRACe:CLEar\r\n")  # Clear the buffer
        self._ser.write((":TRACe:POINTs " + str(no_of_samples) + "\r\n").encode())  # Clear the buffer
        self._ser.write((":TRIG:COUNT " + str(no_of_samples) + "\r\n").encode())  # Clear the buffer
        self._ser.write(b":TRAC:FEED:CONT NEXT\r\n")  # Enable buffer storage, fills buffer then stops
        self._ser.write(b":INIT\r\n")
        # :TRACE:CLEAR
        # :TRACE:POINTS 1000
        # :TRIG:COUNT 1000
        # :TRACE:FEED:CONT NEXT

    def get_raw_values(self):
        # self._ser.write(b':TRACe:POINts:ACTual?\r\n') #Check how many data points live in the buffer
        self._ser.write(b":TRACe:DATA?\r\n")

    def get_mean(self):
        self._ser.write(b":CALC2:STATe ON\r\n")
        self._ser.write(b":CALC2:FORM MEAN\r\n")
        self._ser.write(b":CALC2:DATA?\r\n")

    def get_std(self):
        self._ser.write(b":CALC2:FORM SDEV\r\n")
        self._ser.write(b":CALC2:DATA?\r\n")

    def read(self, time_to_wait):
        # print("Reading...")
        while self._ser.inWaiting() < 1:  # If less than 1 byte, don't do anything
            pass
        data = self._ser.read(self._ser.inWaiting())  # Read all the waiting bytes
        # print(data)

        return data

    def check_compliance(self):
        self._ser.write((":SENS:" + self._measure + ":PROT:TRIPped?" + "\r\n").encode("utf-8"))
