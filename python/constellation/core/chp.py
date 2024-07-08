"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import time
import io
import msgpack  # type: ignore[import-untyped]
import zmq
from typing import Tuple
from .protocol import Protocol


def CHPDecodeMessage(buf: bytes) -> Tuple[str, msgpack.Timestamp, int, int]:
    """Decode a CHP binary message.

    Returns host, timestamp, state and interval.

    """
    unpacker = msgpack.Unpacker()
    unpacker.feed(buf)
    protocol = unpacker.unpack()
    host = unpacker.unpack()
    timestamp = unpacker.unpack()
    state = unpacker.unpack()
    interval = unpacker.unpack()
    if not protocol == Protocol.CHP.value:
        raise RuntimeError(f"Received message with malformed CHP header: {protocol}!")
    return host, timestamp, state, interval


class CHPTransmitter:
    """Send and receive via the Constellation Heartbeat Protocol (CHP)."""

    def __init__(self, name: str, socket: zmq.Socket):  # type: ignore[type-arg]
        """Initialize transmitter."""
        self.name = name
        self._socket = socket

    def send(self, state: int, interval: int, flags: int = 0) -> None:
        """Send state and interval via CHP."""
        stream = io.BytesIO()
        packer = msgpack.Packer()
        stream.write(packer.pack(Protocol.CHP))
        stream.write(packer.pack(self.name))
        stream.write(packer.pack(msgpack.Timestamp.from_unix_nano(time.time_ns())))
        stream.write(packer.pack(state))
        stream.write(packer.pack(interval))
        self._socket.send(stream.getbuffer(), flags=flags)

    def recv(
        self, flags: int = zmq.NOBLOCK
    ) -> Tuple[str, msgpack.Timestamp, int, int] | Tuple[None, None, None, None]:
        """Receive a heartbeat via CHP."""
        try:
            buf = self._socket.recv(flags)
        except zmq.ZMQError as e:
            if "Resource temporarily unavailable" not in e.strerror:
                raise RuntimeError(
                    "CommandTransmitter encountered zmq exception"
                ) from e
            return None, None, None, None
        return CHPDecodeMessage(buf)

    def close(self) -> None:
        """Close the socket of the transmitter."""
        self._socket.close()
