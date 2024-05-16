# H. Wennlöf, based on code from C. Riegel
# 2018

from serial import Serial
import time

Units = {
    "Voltage": {"mV": 0.001, "V": 1.0},
    "Current": {"nA": 0.000000001, "uA": 0.000001, "mA": 0.001, "A": 1.0},
}

Mode = {
    "Source": {"V": "VOLT", "v": "VOLT", "I": "CURR", "i": "CURR"},
    "Measure": {"V": "VOLT", "v": "VOLT", "I": "CURR", "i": "CURR"},
}


# Class for the Keithley SMU 2400/2410 series
class KeithleySMU2400Series:
    ser = None

    def __init__(self, conf):
        self.configuration_file = conf
        self.set_device_configuration()

    # ===========================================================================
    # Open serial interface
    # ===========================================================================
    def open_device_interface(self):
        self._ser.open()
        print(
            "Device Ready at Port "
            + self.configuration_file["Device"]["Configuration"]["Port"]
        )

    # ===========================================================================
    # Switch on the output
    # ===========================================================================
    def enable_output(self):
        self._ser.write(b":OUTPUT ON\r\n")
        print("Output On")

    def disable_output(self):
        self._ser.write(b":OUTPUT OFF\r\n")
        print("Output Off")

    # ===========================================================================
    # Close serial interface
    # ===========================================================================
    def close_device_interface(self):
        self._ser.close()
        print(
            "Device Closed at Port "
            + self.configuration_file["Device"]["Configuration"]["Port"]
        )

    # ===========================================================================
    # Do initial configuration
    # ===========================================================================
    def set_device_configuration(self):
        # Initialization of the Serial interface
        try:
            self._ser = Serial(
                port=self.configuration_file["Device"]["Configuration"]["Port"],
                baudrate=self.configuration_file["Device"]["Configuration"]["Baudrate"],
                timeout=2,
                parity="E",
            )
            self._source = Mode["Source"][
                self.configuration_file["Device"]["Configuration"]["Source"]
            ]
            self._measure = Mode["Measure"][
                self.configuration_file["Device"]["Configuration"]["Measure"]
            ]

            # Specifies the size of data buffer
            self._triggerCount = self.configuration_file["Device"]["Configuration"][
                "TriggerCount"
            ]

            # Specifies trigger delay in seconds
            self._triggerDelay = self.configuration_file["Device"]["Configuration"][
                "TriggerDelay"
            ]

            # Specifies source and measurement
            self._rangeSource = self.configuration_file["Device"]["Configuration"][
                "RangeSource"
            ]
            self._autorangeSource = self.configuration_file["Device"]["Configuration"][
                "AutoRangeSource"
            ]
            self._OVPSource = self.configuration_file["Device"]["Configuration"][
                "OVPSource"
            ]
            self._MinAllowedSource = self.configuration_file["Device"]["Configuration"][
                "MinAllowedSource"
            ]
            self._MaxAllowedSource = self.configuration_file["Device"]["Configuration"][
                "MaxAllowedSource"
            ]
            self._SafeLevelSource = self.configuration_file["Device"]["Configuration"][
                "SafeLevelSource"
            ]

            self._rangeMeasure = self.configuration_file["Device"]["Configuration"][
                "RangeMeasure"
            ]
            self._autorangeMeasure = self.configuration_file["Device"]["Configuration"][
                "AutoRangeMeasure"
            ]
            self._complianceMeasure = self.configuration_file["Device"][
                "Configuration"
            ]["ComplianceMeasure"]

            # Set up the source
            self._ser.write(("*rst" + "\r\n").encode("utf-8"))
            self._ser.write((":SYST:BEEP:STAT OFF" + "\r\n").encode("utf-8"))
            self._ser.write((":SOUR:CLEar:IMMediate" + "\r\n").encode("utf-8"))
            self._ser.write(
                (":SOUR:FUNC:MODE " + self._source + "\r\n").encode("utf-8")
            )
            # self._ser.write(b':SOUR:CLEar:IMMediate\r\n')
            self._ser.write(
                (":SOUR:" + self._source + ":MODE FIX" + "\r\n").encode("utf-8")
            )
            self._ser.write(
                (
                    ":SOUR:"
                    + self._source
                    + ":RANG:AUTO "
                    + self._autorangeSource
                    + "\r\n"
                ).encode("utf-8")
            )
            self._ser.write(
                (
                    ":SOUR:"
                    + self._source
                    + ":PROT:LEV "
                    + str(self._OVPSource)
                    + "\r\n"
                ).encode("utf-8")
            )
            if self._autorangeSource == "OFF":
                self._ser.write(
                    (
                        ":SOUR:"
                        + self._source
                        + ":RANG "
                        + str(self._rangeSource)
                        + "\r\n"
                    ).encode("utf-8")
                )
            else:
                None

            # Set up the sensing
            self._ser.write((':SENS:FUNC "' + self._measure + '"\r\n').encode("utf-8"))
            self._ser.write(
                (
                    ":SENS:"
                    + self._measure
                    + ":PROT:LEV "
                    + str(self._complianceMeasure)
                    + "\r\n"
                ).encode("utf-8")
            )
            self._ser.write(
                (
                    ":SENS:"
                    + self._measure
                    + ":RANG:AUTO "
                    + str(self._autorangeMeasure)
                    + "\r\n"
                ).encode("utf-8")
            )

            # Set up the buffer
            self._ser.write(b":TRAC:FEED:CONT NEVer\r\n")  # Disable buffer storage
            self._ser.write(
                b":TRAC:FEED SENSE\r\n"
            )  # Put raw readings on the readout buffer
            self._ser.write(
                (":TRAC:POIN " + str(self._triggerCount) + "\r\n").encode("utf-8")
            )  # Specifies the size of the buffer
            self._ser.write(b":TRAC:CLEar\r\n")  # Clears the ubffer
            self._ser.write(b":TRAC:FEED:CONT NEXT\r\n")  # Enable buffer storage

            # Set up the data format for transfer of readings over bus
            self._ser.write(b":FORMat:DATA ASCii\r\n")
            self._ser.write(
                b":FORMat:ELEM VOLTage, CURRent\r\n"
            )  # Specifies item list to send

            # Set up the trigger
            self._ser.write(
                str.encode(":TRIG:COUN " + str(self._triggerCount) + "\r\n")
            )  # Specifies the number of measurements to do
            self._ser.write(
                str.encode(":TRIG:DELay " + str(self._triggerDelay) + "\r\n")
            )

            print(
                "Device at Port "
                + self.configuration_file["Device"]["Configuration"]["Port"]
                + " Configured"
            )

        except ValueError:
            print("ERROR: No serial connection. Check cable and port!")

    # Reset and disconnect
    def disconnect(self):
        self.reset()
        self._ser.close()

    def enable_auto_range(self):
        self._ser.write(b":SENS:RANG:AUTO ON\r\n")

    def disable_auto_range(self):
        self._ser.write(b":SENS:RANG:AUTO OFF\r\n")

    def reset(self):
        self._ser.write(b"*RST\r\n")

    def set_value(self, source_value):
        if source_value > self._MaxAllowedSource:
            print("ERROR: Source value is higher than Compliance!")
        else:
            self._ser.write(
                str.encode(":SOUR:" + self._source + ":LEVel " + source_value + "\r\n")
            )
            time.sleep(
                self.configuration_file["Device"]["Configuration"]["SettlingTime"]
            )

    def set_voltage(self, voltage_value, unit):
        val = voltage_value * Units["Voltage"][unit]
        if val > self._MaxAllowedSource or val < self._MinAllowedSource:
            raise ValueError("Voltage out of bounds")
        else:
            val = voltage_value * Units["Voltage"][unit]
            self._ser.write(
                str.encode(":SOUR:" + self._source + ":LEVel " + str(val) + "\r\n")
            )
            time.sleep(
                self.configuration_file["Device"]["Configuration"]["SettlingTime"]
            )
            print("Output voltage set to " + str(val))

    def set_source_upper_range(self, senseUpperRange):
        self._ser.write(
            str.encode(
                ":SENSE:" + self._source + ":RANG:UPP " + senseUpperRange + "\r\n"
            )
        )

    # Read from the buffer
    def sample(self):
        self._ser.write(b":TRAC:FEED:CONT NEVer\r\n")
        self._ser.write(b":TRACe:CLEar\r\n")
        self._ser.write(b":TRAC:FEED:CONT NEXT\r\n")
        self._ser.write(b":INIT\r\n")

    def get_raw_values(self):
        self._ser.write(b":TRACe:DATA?\r\n")

    def get_mean(self):
        self._ser.write(b":CALC3:FORM MEAN\r\n")
        self._ser.write(b":CALC3:DATA?\r\n")

    def get_std(self):
        self._ser.write(b":CALC3:FORM SDEV\r\n")
        self._ser.write(b":CALC3:DATA?\r\n")

    def read(self, time_to_wait):
        while self._ser.inWaiting() <= 2:
            pass
        time.sleep(time_to_wait)
        data = self._ser.read(self._ser.inWaiting())
        return data

    def check_compliance(self):
        self._ser.write(
            (":SENS:" + self._measure + ":PROT:TRIPped?" + "\r\n").encode("utf-8")
        )

    # ===========================================================================
    # Returns a list with format [voltage,current]
    # ===========================================================================

    def get_value(self, with_error=False):
        self.sample()
        self.get_mean()
        dmean = eval(
            self.read(self.configuration_file["Device"]["Configuration"]["WaitRead"])
        )
        if with_error:
            self.get_std()
            dstd = eval(
                self.read(
                    self.configuration_file["Device"]["Configuration"]["WaitRead"]
                )
            )
            return dmean, dstd
        else:
            return dmean

    def get_voltage(self):
        self.sample()
        self.get_mean()
        d = eval(
            (str(self.read(0.1)).split(",")[0]).split("'")[-1]
        )  # Need to strip away the leading b', to get just the numbers
        self.get_std()
        derr = eval((str(self.read(0.1)).split(",")[0]).split("'")[-1])
        return d, derr

    def get_current(self):
        self.sample()
        self.get_mean()
        d = float(str(self.read(0.1)).split(",")[1]) / 0.000001  # Conversion to uA
        self.get_std()
        derr = float(str(self.read(0.1)).split(",")[1]) / 0.000001  # Conversion to uA
        return d, derr

    def state(self):
        print("If the script stops here, the output is turned off\n.")
        print("Output voltage:", self.get_voltage()[0], "V")
        # 2 : Read-Voltage
        print("Output current:", self.get_current()[0], "uA")
        # 3 : Current

    # ramps the voltage, from current voltage to the given target one, with a step.
    def ramp_v(self, v_target, v_step, unit):
        currVoltage = self.get_voltage()[0]
        print(
            "Ramping output from "
            + str(currVoltage)
            + "V to "
            + str(v_target)
            + str(unit)
        )
        while (currVoltage - v_step) > v_target:  # Ramping down
            try:
                self.set_voltage(currVoltage - v_step, unit)
            except ValueError:
                print(
                    "Error occurred. Check compliance and voltage. Going to safe state"
                )
                self.set_voltage(self._SafeLevelSource, unit)
                raise ValueError("Voltage ramp failed")
            time.sleep(0.1)
            currVoltage = self.get_voltage()[0]
        while (currVoltage + v_step) < v_target:  # Ramping up
            try:
                self.set_voltage(currVoltage + v_step, unit)
            except ValueError:
                print(
                    "Error occurred. Check compliance and voltage. Going to safe state"
                )
                self.set_voltage(self._SafeLevelSource, unit)
                raise ValueError("Voltage ramp failed")
            time.sleep(0.1)
            currVoltage = self.get_voltage()[0]
        try:
            self.set_voltage(v_target, unit)
        except ValueError:
            print("Error occurred. Check compliance and voltage. Going to safe state")
            self.set_voltage(self._SafeLevelSource, unit)
            raise ValueError("Voltage ramp failed")
