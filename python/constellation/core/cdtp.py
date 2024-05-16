#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

Module implementing the Constellation Data Transmission Protocol.
"""

from enum import Enum

import msgpack
import io
import zmq

from .protocol import MessageHeader, Protocol


class CDTPMessageIdentifier(Enum):
    """Defines the message types of the CDTP.

    Part of the Constellation Satellite Data Protocol, see
    docs/protocols/cdtp.md for details.

    """

    DAT = 0x0
    BOR = 0x1
    EOR = 0x2


class CDTPMessage:
    """Class holding details of a received CDTP command."""

    name: str = None
    timestamp: msgpack.Timestamp = None
    msgtype: CDTPMessageIdentifier = None
    sequence_number: int = None
    meta: dict[str, any] = {}
    payload: any = None

    def set_header(self, name, timestamp, meta):
        """Sets information retrieved from a message header."""
        self.name = name
        self.timestamp = timestamp
        self.meta = meta

    def __str__(self):
        """Pretty-print request."""
        s = "Data message from {} received at {} of type {}, "
        s += "sequence number {} {} payload and meta {}."
        return s.format(
            self.name,
            self.timestamp,
            self.msgtype,
            self.sequence_number,
            "with a" if self.payload is not None else "without a",
            self.meta,
        )


class DataTransmitter:
    """Base class for sending Constellation data packets via ZMQ."""

    def __init__(self, name: str, socket: zmq.Socket):
        """Initialize transmitter.

        socket: the ZMQ socket to use if no other is specified on send()/recv()
        calls.

        name: the name to use in the message header.
        """
        self.name = name
        self.msgheader = MessageHeader(name, Protocol.CDTP)
        self._socket = socket
        self.running = False
        self.sequence_number = 0

    def send_start(self, payload, flags: int = 0):
        """
        Send starting message of data run over a ZMQ socket.

        Follows the Constellation Data Transmission Protocol.

        payload: meta information about the beginning of run.

        flags: additional ZMQ socket flags to use during transmission.

        """
        self.sequence_number = 0
        self.running = True
        return self._dispatch(
            run_identifier=CDTPMessageIdentifier.BOR, payload=payload, flags=flags
        )

    def send_data(self, payload, meta: dict = {}, flags: int = 0):
        """
        Send data message of data run over a ZMQ socket.

        Follows the Constellation Data Transmission Protocol.

        payload: meta information about the beginning of run.

        meta: optional dictionary that is sent as a map of string/value
        pairs with the header.

        flags: additional ZMQ socket flags to use during transmission.

        """
        if self.running:
            self.sequence_number += 1
            return self._dispatch(
                run_identifier=CDTPMessageIdentifier.DAT,
                payload=payload,
                meta=meta,
                flags=flags,
            )
        msg = "Data transfer sequence not started"
        raise RuntimeError(msg)

    def send_end(self, payload, flags: int = 0):
        """
        Send ending message of data run over a ZMQ socket.

        Follows the Constellation Data Transmission Protocol.

        payload: meta information about the end of run.

        flags: additional ZMQ socket flags to use during transmission.

        """

        self.running = False
        return self._dispatch(
            run_identifier=CDTPMessageIdentifier.EOR, payload=payload, flags=flags
        )

    def _dispatch(
        self,
        run_identifier: CDTPMessageIdentifier,
        payload: any = None,
        meta: dict = {},
        flags: int = 0,
    ):
        """Send a payload over a ZMQ socket.

        Follows the Constellation Data Transmission Protocol.

        run_identifier: flag identifying start, middle and end of run

        payload: data to send.

        meta: dictionary to include in the map of the message header.

        flags: additional ZMQ socket flags to use during transmission.

        Returns: return of socket.send(payload) call.

        """

        stream = io.BytesIO()
        packer = msgpack.Packer()
        stream.write(packer.pack(run_identifier.value))
        stream.write(packer.pack(self.sequence_number))
        # Set option to send more based on flag input
        flags = zmq.SNDMORE | flags
        # message header
        self.msgheader.send(self._socket, meta=meta, flags=flags)
        if payload is None:
            flags = flags & (~zmq.SNDMORE)  # flip SNDMORE bit
        self._socket.send(
            stream.getbuffer(),
            flags=flags,
        )

        # payload
        if payload is not None:
            flags = flags & (~zmq.SNDMORE)  # flip SNDMORE bit
            self._socket.send(packer.pack(payload), flags=flags)

    def recv(self, flags: int = 0) -> CDTPMessage:
        """Receive a multi-part data transmission.

        Follows the Constellation Data Transmission Protocol.

        flags: additional ZMQ socket flags to use during transmission.

        Returns: CTDPMessage

        """
        try:
            binmsg = self._socket.recv_multipart(flags=flags)
        except zmq.ZMQError:
            return None
        return self.decode(binmsg)

    def decode(self, binmsg) -> CDTPMessage:
        """Decode a binary message into a CTDPMessage."""
        msg = CDTPMessage()
        msg.set_header(*self.msgheader.decode(binmsg[0]))
        unpacker = msgpack.Unpacker()
        unpacker.feed(binmsg[1])

        # Retrieve sequence identifier and number
        try:
            msg.msgtype = CDTPMessageIdentifier(unpacker.unpack())
        except ValueError:
            raise RuntimeError(
                f"Received invalid sequence identifier with msg: {msg.msgtype}"
            )
        msg.sequence_number = unpacker.unpack()

        try:
            # Retrieve payload
            unpacker.feed(binmsg[2])
            msg.payload = unpacker.unpack()
        except IndexError:
            pass
        return msg
