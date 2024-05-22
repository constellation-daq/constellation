"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import time
import io
import msgpack
import zmq
from .protocol import Protocol


class CHPTransmitter:
    """Send and receive via the Constellation Heartbeat Protocol (CHP)."""

    def __init__(self, name: str, socket: zmq.Socket):
        """Initialize transmitter."""
        self.name = name
        self._socket = socket

    def send(self, state: int, interval: int, flags: int = 0):
        """Send state and interval via CHP."""
        stream = io.BytesIO()
        packer = msgpack.Packer()
        stream.write(packer.pack(Protocol.CHP))
        stream.write(packer.pack(self.name))
        stream.write(packer.pack(msgpack.Timestamp.from_unix_nano(time.time_ns())))
        stream.write(packer.pack(state))
        stream.write(packer.pack(interval))
        self._socket.send(stream.getbuffer(), flags=flags)

    def recv(self, flags: int = zmq.NOBLOCK):
        """Receive a heartbeat via CHP."""
        unpacker = msgpack.Unpacker()
        try:
            buf = self._socket.recv(flags)
            unpacker.feed(buf)
        except zmq.ZMQError as e:
            if "Resource temporarily unavailable" not in e.strerror:
                raise RuntimeError(
                    "CommandTransmitter encountered zmq exception"
                ) from e
            return None, None, None, None
        protocol = unpacker.unpack()
        host = unpacker.unpack()
        timestamp = unpacker.unpack()
        state = unpacker.unpack()
        interval = unpacker.unpack()
        if not protocol == Protocol.CHP.value:
            raise RuntimeError(
                f"Received message with malformed CHP header: {protocol}!"
            )
        return host, timestamp, state, interval

    def close(self):
        """Close the socket of the transmitter."""
        self._socket.close()
