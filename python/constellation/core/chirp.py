"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

Module implementing the Constellation Host Identification and Reconnaissance Protocol (CHIRP).
"""

from hashlib import md5
import io
from uuid import UUID
from enum import Enum
from .network import get_broadcast, get_broadcast_socket, decode_ancdata, ANC_BUF_SIZE


CHIRP_PORT = 7123
CHIRP_HEADER = "CHIRP\x01"


def get_uuid(name: str) -> UUID:
    """Return the UUID for a string using MD5 hashing."""
    hash = md5(name.encode(), usedforsecurity=False)
    return UUID(bytes=hash.digest())


class CHIRPServiceIdentifier(Enum):
    """Identifies the type of service.

    The CONTROL service identifier indicates a CSCP (Constellation Satellite
    Control Protocol) service.

    The HEARTBEAT service identifier indicates a CHP (Constellation Heartbeat
    Protocol) service.

    The MONITORING service identifier indicates a CMDP (Constellation Monitoring
    Distribution Protocol) service.

    The DATA service identifier indicates a CDTP (Constellation Data
    Transmission Protocol) service.

    The NONE identifier is used for initialization only, and is not a valid
    service type.

    """

    NONE = 0x0
    CONTROL = 0x1
    HEARTBEAT = 0x2
    MONITORING = 0x3
    DATA = 0x4


class CHIRPMessageType(Enum):
    """Identifies the type of message sent or received via the CHIRP protocol.

    See docs/protocols/chirp.md for details.

    REQUEST: A message with REQUEST type indicates that CHIRP hosts should reply
    with an OFFER

    OFFER: A message with OFFER type indicates that service is available

    DEPART: A message with DEPART type indicates that a service is no longer
    available

    NONE: Value used for initialization only, not a valid message type.

    """

    NONE = 0x0
    REQUEST = 0x1
    OFFER = 0x2
    DEPART = 0x3


class CHIRPMessage:
    """Class to hold a CHIRP message."""

    def __init__(
        self,
        msgtype: CHIRPMessageType = CHIRPMessageType.NONE,
        group_uuid: UUID = UUID(int=0),
        host_uuid: UUID = UUID(int=0),
        serviceid: CHIRPServiceIdentifier = CHIRPServiceIdentifier.NONE,
        port: int = 0,
    ):
        """Initialize attributes."""
        self.msgtype = msgtype
        self.group_uuid = group_uuid
        self.host_uuid = host_uuid
        self.serviceid = serviceid
        self.port = port
        self.from_address: str = ""
        self.dest_address: str = ""

    def pack(self) -> bytes:
        """Serialize message to raw bytes."""
        bytes = io.BytesIO()
        bytes.write(CHIRP_HEADER.encode())
        bytes.write(self.msgtype.value.to_bytes(length=1))
        bytes.write(self.group_uuid.bytes)
        bytes.write(self.host_uuid.bytes)
        bytes.write(self.serviceid.value.to_bytes(length=1))
        bytes.write(self.port.to_bytes(length=2, byteorder="big"))
        return bytes.getvalue()

    def unpack(self, msg: bytes) -> None:
        """Decode from bytes."""
        # Check message length
        if len(msg) != 42:
            raise RuntimeError(
                f"Invalid CHIRP message: length is {len(msg)} instead of 42 bytes long"
            )
        # Check header
        if msg[0:6] != CHIRP_HEADER.encode():
            raise RuntimeError(
                f"Invalid CHIRP message: header {msg[0:6]!r} is malformed"
            )
        # Decode message
        self.msgtype = CHIRPMessageType(int.from_bytes(msg[6:7]))
        self.group_uuid = UUID(bytes=msg[7:23])
        self.host_uuid = UUID(bytes=msg[23:39])
        self.serviceid = CHIRPServiceIdentifier(int.from_bytes(msg[39:40]))
        self.port = int.from_bytes(msg[40:42], byteorder="big")

    def __str__(self) -> str:
        """Pretty-print CHIRP message."""
        s = "CHIRP message from {} received of type {}, "
        s += "host id {}, service id {} on port {}."
        return s.format(
            self.from_address,
            self.msgtype,
            self.host_uuid,
            self.serviceid,
            self.port,
        )


class CHIRPBeaconTransmitter:
    """Class for broadcasting CHRIP messages.

    See docs/protocols/chirp.md for details.

    """

    def __init__(
        self,
        name: str,
        group: str,
        interface: str,
    ) -> None:
        """Initialize attributes and open broadcast socket."""
        self._host_uuid = get_uuid(name)
        self._group_uuid = get_uuid(group)

        # whether or not to filter broadcasts on group
        self._filter_group = True

        # whether or not there are incoming packets
        self.busy = False

        # Create UPP broadcasting socket
        #
        # NOTE: Socket options are often OS-specific; the ones below were chosen
        # for supporting Linux-based systems.
        #
        self._sock = get_broadcast_socket()
        # blocking (i.e. with a timeout for recv calls)
        self._sock.setblocking(True)
        self._sock.settimeout(0.05)
        # determine to what address(es) to send broadcasts to
        self._broadcast_addrs = list(get_broadcast(interface))
        # bind to specified interface(s) to listen to incoming broadcast.
        # NOTE: only support for IPv4 is implemented
        if interface == "*":
            # INADDR_ANY for IPv4
            interface = ""
        else:
            # use broadcast address instead
            interface = self._broadcast_addrs[0]
        # one socket for listening
        self._sock.bind((interface, CHIRP_PORT))

    @property
    def host(self) -> UUID:
        """Get the UUID of the host this transmitter was set up for."""
        return self._host_uuid

    @property
    def group(self) -> UUID:
        """Get the UUID of the Constellation group of this transmitter."""
        return self._group_uuid

    @property
    def filter(self) -> bool:
        """Whether or not incoming broadcasts are filtered on group."""
        return self._filter_group

    @filter.setter
    def filter(self, val: bool) -> None:
        """Whether or not incoming broadcasts are filtered on group."""
        self._filter_group = val

    def broadcast(
        self,
        serviceid: CHIRPServiceIdentifier,
        msgtype: CHIRPMessageType,
        port: int = 0,
        dest_address: str = "",
    ) -> None:
        """Broadcast a given service."""
        msg = CHIRPMessage(msgtype, self._group_uuid, self._host_uuid, serviceid, port)
        self.busy = True
        if not dest_address:
            # send to all
            for bcast in self._broadcast_addrs:
                self._sock.sendto(msg.pack(), (bcast, CHIRP_PORT))
        else:
            self._sock.sendto(msg.pack(), (dest_address, CHIRP_PORT))
        self.busy = False

    def listen(self) -> CHIRPMessage | None:
        """Listen in on CHIRP port and return message if data was received."""
        try:
            buf, ancdata, flags, from_address = self._sock.recvmsg(
                1024,
                ANC_BUF_SIZE,
            )
            self.busy = True
        except (BlockingIOError, TimeoutError):
            # no data waiting for us
            self.busy = False
            return None

        # Unpack msg
        msg = CHIRPMessage()
        try:
            msg.unpack(buf)
        except Exception as e:
            raise RuntimeError(
                f"Received malformed message by host {from_address}: {e}"
            ) from e

        # ignore msg from this (our) host
        if self._host_uuid == msg.host_uuid:
            return None

        # optionally drop messages from other groups
        if self._filter_group and self._group_uuid != msg.group_uuid:
            return None

        msg.from_address = from_address[0]
        msg.dest_address, _destport = decode_ancdata(ancdata)
        return msg

    def close(self) -> None:
        """Close the socket."""
        self._sock.close()
