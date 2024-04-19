#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

Module implementing the Constellation communication protocols.
"""

import msgpack
import zmq

import io
import time
import platform
from enum import StrEnum


class Protocol(StrEnum):
    CDTP = "CDTP\x01"
    CSCP = "CSCP\x01"
    CMDP = "CMDP\x01"
    CHP = "CHP\x01"


class MessageHeader:
    """Class implementing a Constellation message header."""

    def __init__(self, name: str, protocol: Protocol):
        self.name = name
        self.protocol = protocol

    def send(self, socket: zmq.Socket, flags: int = zmq.SNDMORE, meta: dict = None):
        """Send a message header via socket.

        meta is an optional dictionary that is sent as a map of string/value
        pairs with the header.

        Returns: return value from socket.send().

        """
        return socket.send(self.encode(meta), flags)

    def recv(self, socket: zmq.Socket, flags: int = 0):
        """Receive header from socket and return all decoded fields."""
        return self.decode(socket.recv())

    def decode(self, header):
        """Decode header string and return host, timestamp and meta map."""
        unpacker = msgpack.Unpacker()
        unpacker.feed(header)
        protocol = unpacker.unpack()
        host = unpacker.unpack()
        timestamp = unpacker.unpack()
        meta = unpacker.unpack()
        if not protocol == self.protocol.value:
            raise RuntimeError(
                f"Received message with malformed {self.protocol.name} header: {header}!"
            )
        return host, timestamp, meta

    def encode(self, meta: dict = None):
        """Generate and return a header as list."""
        if not meta:
            meta = {}
        stream = io.BytesIO()
        packer = msgpack.Packer()
        stream.write(packer.pack(self.protocol.value))
        stream.write(packer.pack(self.name))
        stream.write(packer.pack(msgpack.Timestamp.from_unix_nano(time.time_ns())))
        stream.write(packer.pack(meta))
        return stream.getbuffer()


class DataTransmitter:
    """Base class for sending Constellation data packets via ZMQ."""

    def __init__(self, socket: zmq.Socket = None, host: str = None):
        """Initialize transmitter.

        socket: the ZMQ socket to use if no other is specified on send()/recv()
        calls.

        host: the name to use in the message header. Defaults to system host name.

        """
        if not host:
            host = platform.node()
        self.host = host
        self._socket = socket

    def send(
        self, payload, meta: dict = None, socket: zmq.Socket = None, flags: int = 0
    ):
        """Send a payload over a ZMQ socket.

        Follows the Constellation Data Transmission Protocol.

        payload: data to send.

        meta: dictionary to include in the map of the message header.

        socket: ZMQ socket to use for transmission. If none is specified, use
        the one the class was initialized with.

        flags: additional ZMQ socket flags to use during transmission.

        Returns: return of socket.send(payload) call.

        """
        if not meta:
            meta = {}
        # use default socket if none was specified
        if not socket:
            socket = self._socket
        flags = zmq.SNDMORE | flags
        # message header
        socket.send(msgpack.packb(Protocol.CDTP), flags=flags)
        socket.send(msgpack.packb(self.host), flags=flags)
        socket.send(msgpack.packb(time.time_ns()), flags=flags)
        socket.send(msgpack.packb(meta), flags=flags)
        # payload
        flags = flags & (~zmq.SNDMORE)  # flip SNDMORE bit
        return socket.send(msgpack.packb(payload), flags=flags)

    def recv(self, socket: zmq.Socket = None, flags: int = 0):
        """Receive a multi-part data transmission.

        Follows the Constellation Data Transmission Protocol.

        socket: ZMQ socket to use for transmission. If none is specified, use
        the one the class was initialized with.

        flags: additional ZMQ socket flags to use during transmission.

        Returns: payload, map (meta data), timestamp and sending host.

        """
        # use default socket if none was specified
        if not socket:
            socket = self._socket
        msg = socket.recv_multipart(flags=flags)
        if not len(msg) == 5:
            raise RuntimeError(
                f"Received message with wrong length of {len(msg)} parts!"
            )
        if not msgpack.unpackb(msg[0]) == Protocol.CDTP:
            raise RuntimeError(
                f"Received message with malformed CDTP header: {msgpack.unpackb(msg[0])}!"
            )
        payload = msgpack.unpackb(msg[4])
        meta = msgpack.unpackb(msg[3])
        ts = msgpack.unpackb(msg[2])
        host = msgpack.unpackb(msg[1])
        return payload, meta, host, ts

    def close(self):
        """Close the socket."""
        self._socket.close()
