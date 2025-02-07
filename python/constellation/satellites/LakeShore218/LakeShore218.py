"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides the class for the LakeShore218 satellite.
"""

from typing import Any
from threading import Lock

from constellation.core.commandmanager import cscp_requestable
from constellation.core.configuration import Configuration
from constellation.core.cmdp import MetricsType
from constellation.core.cscp import CSCPMessage
from constellation.core.monitoring import schedule_metric
from constellation.core.satellite import Satellite

import pyvisa
import pyvisa.constants


class LakeShore218(Satellite):
    _serial: pyvisa.resources.SerialInstrument

    def __init__(self, *args, **kwargs):
        self._rm = pyvisa.ResourceManager()
        self._lock = Lock()
        super().__init__(*args, **kwargs)

    def do_initializing(self, config: Configuration) -> None:
        port = config["port"]
        visa_address = f"ASRL{port}::INSTR"
        self.log.debug("Opening VISA device %s", visa_address)

        self._serial = self._rm.open_resource(  # type: ignore
            visa_address,
            baud_rate=9600,
            data_bits=7,
            stop_bits=pyvisa.constants.StopBits.one,
            parity=pyvisa.constants.Parity.odd,
            read_termination="\r\n",
            write_termination="\r\n",
        )

    def _get_temp(self, channel: int) -> float | None:
        # Check that _serial exists (required for metrics before INIT)
        if not hasattr(self, "_serial"):
            return None
        # Lock required since serial not thread safe
        with self._lock:
            # Check if input is enabled (return "0" or "1")
            if self._serial.query(f"INPUT? {channel}") == "1":
                return float(self._serial.query(f"KRDG? {channel}"))
        # Return None if not enabled
        return None

    @cscp_requestable
    def get_temp(self, request: CSCPMessage) -> tuple[str, Any, dict[str, Any]]:
        channel = int(request.payload)
        if channel < 1 or channel > 8:
            raise Exception(f"Channel {channel} does not exist")
        temp = self._get_temp(channel)
        verb = f"{temp}K" if temp else "Disabled"
        return verb, temp, {}

    @schedule_metric("K", MetricsType.LAST_VALUE, 5)
    def temp_1(self) -> Any:
        return self._get_temp(1)

    @schedule_metric("K", MetricsType.LAST_VALUE, 5)
    def temp_2(self) -> Any:
        return self._get_temp(2)

    @schedule_metric("K", MetricsType.LAST_VALUE, 5)
    def temp_3(self) -> Any:
        return self._get_temp(3)

    @schedule_metric("K", MetricsType.LAST_VALUE, 5)
    def temp_4(self) -> Any:
        return self._get_temp(4)

    @schedule_metric("K", MetricsType.LAST_VALUE, 5)
    def temp_5(self) -> Any:
        return self._get_temp(5)

    @schedule_metric("K", MetricsType.LAST_VALUE, 5)
    def temp_6(self) -> Any:
        return self._get_temp(6)

    @schedule_metric("K", MetricsType.LAST_VALUE, 5)
    def temp_7(self) -> Any:
        return self._get_temp(7)

    @schedule_metric("K", MetricsType.LAST_VALUE, 5)
    def temp_8(self) -> Any:
        return self._get_temp(8)
