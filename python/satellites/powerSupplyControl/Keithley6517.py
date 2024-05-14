from serial import Serial
import time

Units = {
    "Voltage": {"mV": 0.001, "V": 1.0},
    "Current": {"nA": 0.000000001, "uA": 0.000001, "mA": 0.001, "A": 1.0},
}

Mode = {
    "Measure": {
        "V": "VOLT",
        "v": "VOLT",
        "I": "CURR",
        "i": "CURR",
        "R": "RES",
        "r": "RES",
    }
}


# Class for the Keithley 6517 series
class KeithleySMU6517Series:
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
                timeout=2,  # ,
                # parity = 'E',
                # rtscts = False,
                # xonxoff = False,
                # stopbits = 2
            )
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
            # self._ser.write((':SYST:PRESet' + "\r\n").encode('utf-8'))
            self._ser.write((":SYST:ZCH OFF" + "\r\n").encode("utf-8"))
            self._ser.write((":CALC:FORM NONE" + "\r\n").encode("utf-8"))

            self._ser.write(
                (":SOUR:VOLT:LIM " + str(self._OVPSource) + "\r\n").encode("utf-8")
            )

            # Set up the sensing. Can be voltage, current, or resistance
            self._ser.write((':SENS:FUNC "' + self._measure + '"\r\n').encode("utf-8"))
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
                (":TRAC:POINt " + str(self._triggerCount) + "\r\n").encode("utf-8")
            )  # Specifies the size of the buffer
            self._ser.write(b":TRAC:CLEar\r\n")  # Clears the buffer
            self._ser.write(
                b":TRAC:FEED:CONT NEXT\r\n"
            )  # Enable buffer storage. Fills the buffer, then stops

            # Set up the data format for transfer of readings over bus
            self._ser.write(b":FORMat:DATA ASCii\r\n")
            self._ser.write(
                b":FORMat:ELEM READing, TSTamp, VSOurce\r\n"
            )  # Specifies item list to send. Defaults to READing, CHANnel, RNUMber, UNITs, TSTamp, STATus
            self._ser.write(
                b":TRACe:ELEM VSOurce, TSTamp\r\n"
            )  # Specifies item list to send. Defaults to READing, CHANnel, RNUMber, UNITs, TSTamp, STATus

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

    def reset(self):
        self._ser.write(b"*RST\r\n")

    def set_value(self, source_value):
        if source_value > self._MaxAllowedSource:
            print("ERROR: Source value is higher than Compliance!")
        else:
            self._ser.write(str.encode(":SOUR:VOLT:LEVel " + source_value + "\r\n"))
            time.sleep(
                self.configuration_file["Device"]["Configuration"]["SettlingTime"]
            )

    def set_voltage(self, voltage_value, unit):
        val = voltage_value * Units["Voltage"][unit]
        if val > self._MaxAllowedSource or val < self._MinAllowedSource:
            raise ValueError("Voltage out of bounds")
        else:
            val = voltage_value * Units["Voltage"][unit]
            self._ser.write(str.encode(":SOUR:VOLT:LEVel " + str(val) + "\r\n"))
            time.sleep(
                self.configuration_file["Device"]["Configuration"]["SettlingTime"]
            )
            print("Output voltage set to " + str(val))

    def set_source_upper_range(self, senseUpperRange):
        self._ser.write(str.encode(":SENSE:VOLT:RANG:UPP " + senseUpperRange + "\r\n"))

    # Read from the buffer
    def sample(self, no_of_samples):
        self._ser.write(b":TRAC:FEED:CONT NEVer\r\n")  # Disable buffer storage
        self._ser.write(b":TRACe:CLEar\r\n")  # Clear the buffer
        self._ser.write(
            (":TRACe:POINTs " + str(no_of_samples) + "\r\n").encode()
        )  # Clear the buffer
        self._ser.write(
            (":TRIG:COUNT " + str(no_of_samples) + "\r\n").encode()
        )  # Clear the buffer
        self._ser.write(
            b":TRAC:FEED:CONT NEXT\r\n"
        )  # Enable buffer storage, fills buffer then stops
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
        while self._ser.inWaiting() <= 2:  # If less than 1 byte, don't do anything
            pass
        time.sleep(time_to_wait)
        data = self._ser.read(self._ser.inWaiting())  # Read all the waiting bytes
        # print(data)

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

    def get_current_timestamp_voltage(self):
        self.sample(1)
        time.sleep(
            2
        )  # Wait for values to enter buffer properly. TODO: it working or not is dependent on this, currently...
        self.get_raw_values()
        valueList = self.read(0.1).decode("utf-8").split(",")
        print(valueList)
        current = float(valueList[0])
        timestamp = float(valueList[1])
        voltage = float(valueList[2])
        # self.get_std()
        # derr = eval((str(self.read(0.1)).split(",")[0]).split("'")[-1])
        return current, timestamp, voltage

    # Get the current reading, callable for the publisher
    def get_current(self):
        self.sample(1)
        time.sleep(
            2
        )  # Wait for values to enter buffer properly. TODO: it working or not is dependent on this, currently...
        self.get_raw_values()
        valueList = self.read(0.1).decode("utf-8").split(",")
        print(valueList)
        current = float(valueList[0])
        # timestamp = float(valueList[1])
        # voltage = float(valueList[2])
        # self.get_std()
        # derr = eval((str(self.read(0.1)).split(",")[0]).split("'")[-1])
        return current, "A"

    # Get the voltage reading, callable for the publisher
    def get_voltage(self):
        self.sample(1)
        time.sleep(
            2
        )  # Wait for values to enter buffer properly. TODO: it working or not is dependent on this, currently...
        self.get_raw_values()
        valueList = self.read(0.1).decode("utf-8").split(",")
        print(valueList)
        # current = float(valueList[0])
        # timestamp = float(valueList[1])
        voltage = float(valueList[2])
        # self.get_std()
        # derr = eval((str(self.read(0.1)).split(",")[0]).split("'")[-1])
        return voltage, "V"

    def state(self):
        print("If the script stops here, the output is turned off\n.")
        print("Output voltage:", self.get_voltage()[0], "V")
        # 2 : Read-Voltage
        print("Output current:", self.get_current()[0], "uA")
        # 3 : Current

    # ramps the voltage, from current voltage to the given target one, with a step.
    def ramp_v(self, v_target, v_step, unit):
        currVoltage = self.get_current_timestamp_voltage()[2]
        print(
            "Ramping output from "
            + str(currVoltage)
            + "V to "
            + str(v_target)
            + str(unit)
            + " in steps of "
            + str(v_step)
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
            currVoltage = self.get_current_timestamp_voltage()[2]
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
            currVoltage = self.get_current_timestamp_voltage()[2]
        try:
            self.set_voltage(v_target, unit)
        except ValueError:
            print("Error occurred. Check compliance and voltage. Going to safe state")
            self.set_voltage(self._SafeLevelSource, unit)
            raise ValueError("Voltage ramp failed")

    def simpleFill(self):
        self._ser.write(b"*RST\r\n")
        self._ser.write(b":TRACE:CLEAR\r\n")
        self._ser.write(b":TRAC:POINTS 1000\r\n")
        self._ser.write(b":TRIG:COUNT 1000\r\n")
        self._ser.write(b":SENSE:FUNC CURR\r\n")
        self._ser.write(b":SENSE:CURR:RANGE 1e-6\r\n")
        self._ser.write(b":SENSE:CURR:AVER:STAT OFF\r\n")
        self._ser.write(b":SENSE:CURR:MED:STAT OFF\r\n")
        self._ser.write(b":TRIG:DEL 0.0\r\n")
        self._ser.write(b":SENSE:CURR:NPLC 0.01\r\n")
        self._ser.write(b":FORM:ELEM READ, TST\r\n")
        self._ser.write(b":CALC:STAT OFF\r\n")
        self._ser.write(b":SYSTEM:LSYN:STAT OFF\r\n")
        self._ser.write(b":DISP:ENAB OFF\r\n")
        self._ser.write(b":TRAC:FEED:CONT NEXT\r\n")
        self._ser.write(b":SYST:ZCH OFF\r\n")
        self._ser.write(b":INIT\r\n")
        # *RST
        # :TRACE:CLEAR
        # :TRAC:POINTS 1000
        # :TRIG:COUNT 1000
        # :SENSE:FUNC 'CURR'
        # :SENSE:CURR:RANGE 1e-6
        # :SENSE:CURR:AVER:STAT OFF
        # :SENSE:CURR:MED:STAT OFF
        # :TRIG:DEL 0.0
        # :SENSE:CURR:NPLC 0.01
        # :FORM:ELEM READ, TST
        # :CALC:STAT OFF
        # :SYSTEM:LSYN:STAT OFF
        # :DISP:ENAB OFF
        # :TRAC:FEED:CONT NEXT
        # :SYST:ZCH OFF
        # :INIT
