"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides the classes for communication with CAEN's NDT1470 desktop
HV power supplies.

Supported are USB or TCP/IP and, for compatibility, the pycaenhv API for CAEN's
HV crate series is used whenever possible.

"""

import socket
import time
import threading
from typing import List, Any, Dict
import serial  # type: ignore[import-untyped]

# available parameters (to monitor) in the NDT1470
# FIXME : this could probably be determined at runtime via request sent to device
PARAMETERS_GET = [
    "VSet",
    "VMin",
    "VMax",
    "VDec",
    "VMon",
    "ISet",
    "IMin",
    "IMax",
    "ISDec",
    "IMon",
    "IMRange",
    "IMDec",
    "MaxV",
    "MVMin",
    "MVMax",
    "MVDec",
    "RUp",
    "RUpMin",
    "RUpMax",
    "RUpDec",
    "RDw",
    "RDwMin",
    "RDwMax",
    "RDwDec",
    "Trip",
    "TripMin",
    "TripMax",
    "TripDec",
    "PDwn",
    "Pol",
    "Stat",
]
PARAMETERS_SET = [
    "VSet",
    "ISet",
    "MaxV",
    "RUp",
    "RDw",
    "Trip",
    "PDwn",
    "IMRange",
    "Pw",  # missing in HW but software added; True/False to enable/disable power
    "ON",  # does not take any parameter
    "OFF",  # does not take any parameter
    "BDCLR",
]
NCHANNELS = 4


def status_unpack(n: int) -> list[str]:
    """decodes status bits (see p25 of the NDT1470 user manual (UM2027, rev.17). )"""
    bitfcn = [
        "ON",
        "RUP 1 : Channel Ramp UP",
        "RDW 1 : Channel Ramp DOWN",
        "OVC 1 : IMON >= ISET",
        "OVV 1 : VMON > VSET + 2.5 V",
        "UNV 1 : VMON < VSET – 2.5 V",
        "MAXV 1 : VOUT in MAXV protection",
        "TRIP 1 : Ch OFF via TRIP (Imon >= Iset during TRIP)",
        "OVP 1 : Output Power > Max",
        "OVT 1: TEMP > 105°C",
        "DIS 1 : Ch disabled (REMOTE Mode and Switch on OFF position)",
        "KILL 1 : Ch in KILL via front panel",
        "ILK 1 : Ch in INTERLOCK via front panel",
        "NOCAL 1 : Calibration Error",
    ]
    return [m for i, m in enumerate(bitfcn) if n & (1 << i)]


def alarm_unpack(n: int) -> list[str]:
    """Decodes board alarm status bits (see p26 of the NDT1470 user manual
    (UM2027, rev.17). )"""
    bitfcn = [
        "Ch0 in Alarm status",
        "Ch1 in Alarm status",
        "Ch2 in Alarm status",
        "Ch3 in Alarm status",
        "Board in POWER FAIL",
        "Board in OVER POWER",
        "Internal HV Clock FAIL",
    ]
    return [m for i, m in enumerate(bitfcn) if n & (1 << i)]


class CaenDecode:
    """Decodes the string sent back by the NDT1470. Based on information on p24
    of the NDT1470 user manual (UM2027, rev.17)."""

    def __init__(self, s):
        self.s = s
        # check if string is empty
        if s and "CMD:OK" in s:
            self.ok = True
        else:
            self.ok = False

    @property
    def errmsg(self) -> str:
        """decodes error message"""
        if self.ok:
            return "OK"
        if not self.s:
            return "Received no response"
        err_dict: dict[str, str] = {
            "CMD:ERR": "Wrong command Format or command not recognized",
            "CH:ERR": "Channel Field not present or wrong Channel value",
            "PAR:ERR": "Field parameter not present or parameter not recognized",
            "VAL:ERR": "Wrong set value (<Min or >Max)",
            "LOC:ERR": "Command SET with module in LOCAL mode",
        }
        for err, msg in err_dict.items():
            if err in self.s:
                return msg
        return f"UNKNOWN ERROR: received '{self.s}'"

    @property
    def val(self):
        """decodes values returned from the device"""
        if "VAL:" not in self.s:
            return None
        # get values from string:
        # indicator is 'VAL:', separator ';'
        vstr = self.s[self.s.find("VAL:") + 4 :]
        v = [float(num) if "." in num else int(num) for num in vstr.split(";")]
        # return just number of lonely element
        if len(v) == 1:
            return v[0]
        # else, return whole list
        return v


class CaenNDT1470Manager:
    """Class for interacting with a CAEN NDT1470 NIM HV module.

    Supports ethernet (telnet) interface or serial (USB) connections.

    """

    def __init__(self) -> None:
        self.boards: dict[int, CaenHVBoard] = {}
        self._handle: socket.socket | serial.Serial | None = None
        self._lock = threading.Lock()
        self.connected: bool = False

    def __del__(self) -> None:
        self.disconnect()

    def is_connected(self) -> bool:
        """Return connection status."""
        return self.connected

    @property
    def handle(self) -> socket.socket | serial.Serial | None:
        """Return current handle (socket or Serial)."""
        return self._handle

    def clear_alarm(self) -> None:
        """Clear the alarm state. Not implemented."""
        raise NotImplementedError

    def kill(self) -> None:
        """Kill powered channels."""
        raise NotImplementedError

    def connect(self, system: str, link: str, argument: str, user: str = "", password: str = "") -> None:
        """Connect to a board."""
        if link == "TCPIP":
            self._handle = self._connect_tcp(argument)
        elif link == "USB":
            self._handle = self._connect_usb(argument)
        self.connected = True
        slot = 0  # TODO : can the NDT1470 have board numbers higher than 0?
        model = "NDT1470"  # TODO : check this at runtime
        self.boards[slot] = CaenHVBoard(
            self,
            slot=slot,
            num_channels=NCHANNELS,
            model=model,
            serial_number=0,  # FIXME
            description="NOTIMPLEMENTED",  # FIXME
            firmware_release="NOTIMPLEMENTED",
        )  # FIXME

    def _connect_tcp(self, link_arg: str):
        """establishes the connection via ethernet interface."""
        # Create a TCP/IP socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (link_arg, 1470)  # default port for NDT1470: 1470
        # set timeout
        sock.settimeout(1)  # tcp connection will take time
        # enable keepalive (Linux only)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
        # establish connection
        sock.connect(server_address)
        return sock

    def _connect_usb(self, link_arg: str):
        """connects to the device via serial connection"""
        # make a default (19200,8,N,1) connection
        serial_link = serial.Serial(link_arg, timeout=0)
        return serial_link

    def disconnect(self):
        """Disconnect the module."""
        if self.connected:
            self._handle.close()
            self._handle = None
            self.connected = False

    def command(self, bd: int, ch: int, par: str, val: Any = None) -> Any:
        """Set values for module 'bd', channel 'ch' and parameter 'par' to value 'val'.

        If value is 'None', then the value will be requested
        Module commands have an empty channel number.

        NOTE: The signature of this method differs from the pycaenhv one.
        """
        res = self._send_cmd(ch=ch, par=par, bd=bd, val=val)
        # test whether we received an error back:
        #
        # NOTE This ignores cases where we received *no* response from the
        # device; noticed no response behavior on set cmds which still resulted
        # in the desired change.
        if not res.ok and res.s:
            raise RuntimeError(f"Error for '{par}' of ch {ch}: {res.errmsg}")
        return res.val

    def _send_cmd(self, ch: int, par: str, bd: int = 0, val: Any = None) -> CaenDecode:
        """constructs and sends cmd to device. Wraps result into a CaenDecode object."""
        cmd: str = f"$BD:{bd:02},"
        par = par.upper()
        # if we don't have a channel then this is a module cmd
        chstr = f"CH:{ch:02}," if ch is not None else ""
        if par == "PW":
            # software-added command to the interface; takes True/False as arg
            if val:
                par = "ON"
                val = None
            else:
                par = "OFF"
                val = None
        if val is None:
            if not par == "ON" and not par == "OFF":
                # monitoring
                cmd += f"CMD:MON,{chstr}PAR:{par}"
            else:
                # switching on/off
                cmd += f"CMD:SET,{chstr}PAR:{par}"
        else:
            # setting value
            cmd += f"CMD:SET,{chstr}PAR:{par},VAL:{val}"
        self._send_raw(cmd)
        return CaenDecode(self._receive_raw())

    def _send_raw(self, msg: str):
        """Sends a (raw) command to the device. `msg' is a string that will be
        byte-encoded before sending. From the NDT1470 user manual (UM2027,
        rev.17, p24) :

        The Format of a command string is the following :
        $BD:**,CMD:***,CH*,PAR:***,VAL:***.**<CR, LF >
        The fields that form the command are :
        BD : 0..31 module address (to send the command)
        CMD : MON, SET
        CH : 0..NUMCH (NUMCH=2 for N1570, NUMCH=4 for the other modules)
        PAR : (see parameters tables)
        VAL : (numerical value must have a Format compatible with resolution and range)

        """
        # appends terminating characters (carriage return and line feed)
        data = f"{msg}\r\n".encode()
        if isinstance(self._handle, socket.socket):
            self._handle.sendall(data)
        elif isinstance(self._handle, serial.Serial):
            self._handle.write(data)
        else:
            raise RuntimeError("No device connected!")
        time.sleep(0.1)  # best to wait a bit before continuing..

    def _receive_raw(self) -> str:
        """receives (raw) response from last issued command to the device."""
        # Wait for events...
        time.sleep(0.2)  # best to wait before continuing..
        buffer = ""
        if isinstance(self._handle, socket.socket):
            # TCP/IP
            try:
                buffer = self._handle.recv(1024).decode()
            except socket.error as e:
                raise RuntimeError(f"Socket communication error: {repr(e)}") from e
        elif isinstance(self._handle, serial.Serial):
            # serial
            buffer = self._handle.read(1024).decode()
        else:
            raise RuntimeError("No device connected!")
        return buffer

    def __enter__(self):
        """Acquire the lock to prevent the other threads from interfering."""
        self._lock.acquire()
        return self

    def __exit__(self, exc_type, exc_value, exc_tb):
        """Release the lock to allow other threads access."""
        self._lock.release()


class CaenHVBoard:
    """Represents a single CAEN HV/LV board, determined by `slot`"""

    def __init__(
        self,
        module,
        slot: int,
        num_channels: int,
        model: str = "",
        description: str = "",
        serial_number: int = -1,
        firmware_release=None,
    ):
        self.module = module
        self.description = description
        self.serial_number = serial_number
        self.firmware_release = firmware_release
        self.slot: int = slot
        self.model: str = model
        self.slot = slot
        self.num_channels = num_channels
        # Get the number of channels for this board
        # Populate channels information
        self.channels: List[Channel] = [Channel(self, ch) for ch in range(self.num_channels)]

    @property
    def handle(self) -> socket.socket | serial.Serial | None:
        """Return module handle."""
        return self.module.handle

    def __str__(self) -> str:
        """Pretty-print the module information"""
        fw = f"{self.firmware_release[0]}.{self.firmware_release[1]}"
        main = f"{self.slot} -- {self.model}, {self.description}"
        return f"{main} ({self.serial_number}/FW {fw}): {self.num_channels} channels"


class Channel:
    """Channel in a CAEN HV/LV board"""

    def __init__(self, board, index: int):
        self.board = board
        self.index = index
        self.parameters = self._channel_info()

    def __str__(self) -> str:
        return f"Channel #{self.index}: {self.parameter_names}"

    def __repr__(self) -> str:
        return f"Channel({self.index}, {self.parameters})"

    @property
    def parameter_names(self):
        """List all available parameters"""
        return tuple(self.parameters.keys())

    @property
    def status(self) -> List[str]:
        """Decodes channel status"""
        status_raw: int = self.board.module.command(self.board.slot, self.index, "STAT")
        return status_unpack(status_raw)

    @property
    def name(self) -> str:
        """Get channel name"""
        raise NotImplementedError

    @name.setter
    def name(self, name: str):
        """Set channel name"""
        raise NotImplementedError

    def toggle(self, flag: bool) -> None:
        """Toggle on or off"""
        if flag:
            self.board.module.command(self.board.slot, self.index, "ON")
        else:
            self.board.module.command(self.board.slot, self.index, "OFF")

    def switch_on(self) -> None:
        """switch the channel ON"""
        self.toggle(True)

    def switch_off(self) -> None:
        """switch the channel OFF"""
        self.toggle(False)

    def is_powered(self) -> bool:
        """Returns True if the channel is ON, False otherwise"""
        res = "ON" in self.status
        return bool(res)

    def _channel_info(self) -> dict[str, "ChannelParameter"]:
        """Helper function to assemble channel information."""
        # FIXME replace hard-coded info with info retrieved from module at runtime
        # NOTE : most values are only set to mimic what pycaenhv uses
        res: dict[str, ChannelParameter] = {}
        for par in PARAMETERS_GET:
            res[par] = ChannelParameter(self, par, {"mode": "R"})
        for par in PARAMETERS_SET:
            res[par] = ChannelParameter(self, par, {"mode": "R/W"})
        return res

    def __getattr__(self, name: str) -> Any:
        """Dynamically get attributes"""
        if name in self.parameters and self.parameters[name].mode in ("R", "R/W"):
            return self.parameters[name]
        return None


class ChannelParameter:
    """Parameter of CAEN HV/LV board channel"""

    def __init__(self, channel: Channel, name: str, attributes: Dict) -> None:
        self.channel = channel
        self.name = name
        # Set available attributes for a given parameter
        self.attributes = attributes
        # Remove duplicated or static info
        if "value" in self.attributes:
            del self.attributes["value"]
        if "name" in self.attributes:
            del self.attributes["name"]

    def __str__(self) -> str:
        return f"{self.name}: {self.attributes}"

    def __repr__(self) -> str:
        return f"ChannelParameter({self.__str__()})"

    @property
    def value(self) -> Any:
        """Reads (if possible) the value of a parameter from the board's channel"""
        if "R" in self.attributes["mode"]:
            return self.channel.board.module.command(self.channel.board.slot, self.channel.index, self.name)
        raise ValueError(f"Trying to read write-only parameter {self.name}")

    @value.setter
    def value(self, value: Any) -> None:
        """Writes (if possible) parameter value to the board"""
        if "W" in self.attributes["mode"]:
            self.channel.board.module.command(self.channel.board.slot, self.channel.index, self.name, value)
        else:
            raise ValueError(f"Trying to write read-only parameter {self.name}")

    def __getattr__(self, name: str) -> Any:
        """Dynamically reads preset attributes"""
        if name in self.attributes:
            return self.attributes[name]
        return None
