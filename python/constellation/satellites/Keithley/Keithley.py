"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides the class for the keithley satellite.
"""

from typing import Any

from constellation.core.base import setup_cli_logging, EPILOG
from constellation.core.commandmanager import cscp_requestable
from constellation.core.configuration import Configuration
from constellation.core.cmdp import MetricsType
from constellation.core.cscp import CSCPMessage
from constellation.core.fsm import SatelliteState
from constellation.core.monitoring import schedule_metric
from constellation.core.satellite import Satellite, SatelliteArgumentParser

from .KeithleyInterface import KeithleyInterface
from .Keithley2410 import Keithley2410

_SUPPORTED_DEVICES = {
    "2410": Keithley2410,
}


class Keithley(Satellite):
    device: KeithleyInterface

    def do_initializing(self, config: Configuration) -> None:
        device_name = config["device"]
        if device_name not in _SUPPORTED_DEVICES.keys():
            raise ValueError(f"Device {device_name} not supported")

        self.device = _SUPPORTED_DEVICES[device_name](config["port"])

        self.voltage = config["voltage"]
        self.voltage_step = config["voltage_step"]
        self.settle_time = config["settle_time"]

        self.ovp = config["ovp"]
        self.compliance = config["compliance"]

        # If one terminal, select as default, otherwise get from config
        terminals = self.device.get_terminals()
        if len(terminals) == 1:
            self.terminal = terminals[0]
        else:
            self.terminal = config["terminal"]
            if self.terminal not in terminals:
                raise ValueError(f"{self.terminal} not a valid terminal (choose from {terminals})")

        self.log.info(f"Initializing Keithley {device_name}")
        self.device.initialize()
        identify = self.device.identify()
        if not identify:
            raise ConnectionError("No connection to Keithley")
        self.log.info("Device: %s", identify)

    def do_launching(self) -> str:
        self.device.set_terminal(self.terminal)
        self._set_ovp()
        self._set_compliance()

        # Set voltage to zero in case it wasn't properly reset for some reason
        self.device.enable_output(False)
        self.device.set_voltage(0)

        # Ramp to target voltage
        self.device.enable_output(True)
        self._ramp(self.voltage)

        return f"Keithley at {self.device.get_voltage()}V"

    def do_landing(self) -> str:
        # Ramp to zero, then disable output
        self._ramp(0)
        self.device.enable_output(False)

        return f"Keithley at {self.device.get_voltage()}V (output disabled)"

    def do_reconfigure(self, partial_config: Configuration) -> str:
        config_keys = partial_config.get_keys()

        if "device" in config_keys:
            raise ValueError("Reconfiguring device is not possible")
        if "port" in config_keys:
            raise ValueError("Reconfiguring port is not possible")
        if "terminal" in config_keys:
            raise ValueError("Reconfiguring terminal is not possible")

        if "ovp" in config_keys:
            self.ovp = partial_config["ovp"]
            self._set_ovp()
        if "compliance" in config_keys:
            self.compliance = partial_config["compliance"]
            self._set_compliance()

        if "voltage_step" in config_keys:
            self.voltage_step = partial_config["voltage_step"]
        if "settle_time" in config_keys:
            self.voltage_step = partial_config["settle_time"]

        # If voltage changed, ramp to new voltage
        if "voltage" in config_keys:
            self.voltage = partial_config["voltage"]
            self.log.info(f"Ramping output voltage from {self.device.get_voltage()}V to {self.voltage}V")
            self.device.ramp_voltage(self.voltage, self.voltage_step, self.settle_time)
            self.log.info(f"Ramped output voltage to {self.voltage}V")

        return f"Keithley at {self.device.get_voltage()}V"

    def reentry(self) -> None:
        self.device.release()
        super().reentry()

    def _set_ovp(self):
        self.log.info(f"Setting OVP to {self.ovp}V")
        self.device.set_ovp(self.ovp)
        device_ovp = self.device.get_ovp()
        if device_ovp != self.ovp:
            raise ValueError(f"OVP set to {self.ovp}V but {device_ovp}V was applied (check manual for supported values)")

    def _set_compliance(self):
        self.log.info(f"Setting compliance to {self.compliance}A")
        self.device.set_compliance(self.compliance)
        device_compliance = self.device.get_compliance()
        if device_compliance != self.compliance:
            raise ValueError(f"Compliance set to {self.compliance}A but {device_compliance}A was applied")

    def _ramp(self, voltage: float):
        self.log.info(f"Ramping output voltage from {self.device.get_voltage()} to {voltage}V")
        self.device.ramp_voltage(voltage, self.voltage_step, self.settle_time)
        self.log.info(f"Ramped output voltage to {self.device.get_voltage()}V")

    @cscp_requestable
    def identify(self, request: CSCPMessage) -> tuple[str, Any, dict]:
        """Identify device (includes serial number)"""
        return self.device.identify(), None, {}

    @cscp_requestable
    def in_compliance(self, request: CSCPMessage) -> tuple[str, Any, dict]:
        """Check if current is in compliance"""
        in_compliance = self.device.in_compliance()
        is_str = "is" if in_compliance else "is not"
        return f"Device {is_str} in compliance", in_compliance, {}

    def _in_compliance_is_allowed(self, request: CSCPMessage) -> bool:
        return self.fsm.current_state_value not in [SatelliteState.NEW, SatelliteState.ERROR]

    @cscp_requestable
    def read_output(self, request: CSCPMessage) -> tuple[str, Any, dict]:
        """Read voltage and current output"""
        voltage, current, timestamp = self.device.read_output()
        return f"Output: {voltage}V, {current}A", {"voltage": voltage, "current": current, "timestamp": timestamp}, {}

    def _read_output_is_allowed(self, request: CSCPMessage) -> bool:
        return self.fsm.current_state_value not in [SatelliteState.NEW, SatelliteState.ERROR]

    @schedule_metric("V", MetricsType.LAST_VALUE, 5)
    def VOLTAGE(self) -> Any:
        if self.fsm.current_state_value not in [SatelliteState.NEW, SatelliteState.ERROR]:
            return self.device.read_output()[0]
        return None

    @schedule_metric("A", MetricsType.LAST_VALUE, 5)
    def CURRENT(self) -> Any:
        if self.fsm.current_state_value not in [SatelliteState.NEW, SatelliteState.ERROR]:
            return self.device.read_output()[1]
        return None

    @schedule_metric("", MetricsType.LAST_VALUE, 5)
    def IN_COMPLIANCE(self) -> Any:
        if self.fsm.current_state_value not in [SatelliteState.NEW, SatelliteState.ERROR]:
            return self.device.in_compliance()
        return None


def main(args=None):
    parser = SatelliteArgumentParser(description=main.__doc__, epilog=EPILOG)
    args = vars(parser.parse_args(args))

    # set up logging
    setup_cli_logging(args["name"], args.pop("log_level"))

    # start server with remaining args
    s = Keithley(**args)
    s.run_satellite()
