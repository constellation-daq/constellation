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


def CHPDecodeMessage(msg: list[bytes]) -> Tuple[str, msgpack.Timestamp, int, int, str | None]:
    """Decode a CHP binary message.

    Returns host, timestamp, state, interval and status if available.

    """
    unpacker = msgpack.Unpacker()
    unpacker.feed(msg[0])
    protocol = unpacker.unpack()
    if not protocol == Protocol.CHP.value:
        raise RuntimeError(f"Received message with malformed CHP header: {protocol}!")

    name = unpacker.unpack()
    timestamp = unpacker.unpack()
    state = unpacker.unpack()
    interval = unpacker.unpack()

    status = None
    if len(msg) > 1:
        status = msg[1].decode("utf-8")

    return name, timestamp, state, interval, status


class CHPTransmitter:
    """Send and receive via the Constellation Heartbeat Protocol (CHP)."""

    def __init__(self, name: str, socket: zmq.Socket):  # type: ignore[type-arg]
        """Initialize transmitter."""
        self.name = name
        self._socket = socket

    def send(self, state: int, interval: int, status: str | None = None, flags: int = 0) -> None:
        """Send state and interval via CHP."""
        stream = io.BytesIO()
        packer = msgpack.Packer()
        stream.write(packer.pack(Protocol.CHP))
        stream.write(packer.pack(self.name))
        stream.write(packer.pack(msgpack.Timestamp.from_unix_nano(time.time_ns())))
        stream.write(packer.pack(state))
        stream.write(packer.pack(interval))

        if status:
            flags = zmq.SNDMORE | flags

        self._socket.send(stream.getbuffer(), flags=flags)

        if status:
            flags = flags & ~zmq.SNDMORE
            self._socket.send_string(status, flags)

    def recv(
        self, flags: int = zmq.NOBLOCK
    ) -> Tuple[str, msgpack.Timestamp, int, int, str | None] | Tuple[None, None, None, None]:
        """Receive a heartbeat via CHP."""
        try:
            msg = self._socket.recv_multipart(flags)
        except zmq.ZMQError as e:
            if "Resource temporarily unavailable" not in e.strerror:
                raise RuntimeError("CommandTransmitter encountered zmq exception") from e
            return None, None, None, None
        return CHPDecodeMessage(msg)

    def close(self) -> None:
        """Close the socket of the transmitter."""
        self._socket.close()
