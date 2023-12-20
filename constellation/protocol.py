#!/usr/bin/env python3
"""Module implementing the Constellation communication protocols."""

import msgpack
import zmq
import time
import platform


PROTOCOL_IDENTIFIER = "CDTP%x01"


class DataTransmitter:
    """Base class for sending data via ZMQ."""

    def __init__(self, host: str = None):
        if not host:
            host = platform.node()
        self.host = host

    def send(self, socket: zmq.Socket, payload, meta: dict = None, flags: int = 0):
        """Send a payload over a ZMQ socket.

        Follows the Constellation Data Transmission Protocol.

        Returns: return of socket.send(payload) call.

        """
        if not meta:
            meta = {}
        flags = zmq.SNDMORE | flags
        # message header
        socket.send(msgpack.packb(PROTOCOL_IDENTIFIER), flags=flags)
        socket.send(msgpack.packb(self.host), flags=flags)
        socket.send(msgpack.packb(time.time_ns()), flags=flags)
        socket.send(msgpack.packb(meta), flags=flags)
        # payload
        flags = flags & (~zmq.SNDMORE)  # flip SNDMORE bit
        return socket.send(msgpack.packb(payload), flags=flags)

    def recv(self, socket: zmq.Socket, flags: int = 0):
        """Receive a multi-part data transmission.

        Follows the Constellation Data Transmission Protocol.

        Returns: payload, map (meta data), timestamp and sending host.

        """
        msg = socket.recv_multipart(flags=flags)
        if not len(msg) == 5:
            raise RuntimeError(f"Received message with wrong length of {len(msg)} parts!")
        if not msgpack.unpackb(msg[0]) == PROTOCOL_IDENTIFIER:
            raise RuntimeError(f"Received message with malformed CDTP header: {msgpack.unpackb(msg[0])}!")
        payload = msgpack.unpackb(msg[4])
        meta = msgpack.unpackb(msg[3])
        ts = msgpack.unpackb(msg[2])
        host = msgpack.unpackb(msg[1])
        return payload, meta, host, ts
