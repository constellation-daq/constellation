#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

Module implementing the Constellation Data Transmission Protocol.
"""

from enum import Enum
from typing import Any

import msgpack  # type: ignore[import-untyped]
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

    name: str = ""
    timestamp: msgpack.Timestamp = msgpack.Timestamp(0)
    msgtype: CDTPMessageIdentifier | None = None
    sequence_number: int = -1
    meta: dict[str, Any] = {}
    payload: Any = None

    def set_header(
        self,
        name: str,
        timestamp: msgpack.Timestamp,
        msgtype: int,
        seqno: int,
        meta: dict[str, Any],
    ) -> None:
        """Sets information retrieved from a message header."""
        self.name = name
        self.timestamp = timestamp
        try:
            self.msgtype = CDTPMessageIdentifier(msgtype)
        except ValueError as exc:
            raise RuntimeError(
                f"Received invalid sequence identifier with msg: {msgtype}"
            ) from exc
        self.sequence_number = seqno
        self.meta = meta

    def __str__(self) -> str:
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

    def __init__(self, name: str, socket: zmq.Socket | None):  # type: ignore[type-arg]
        """Initialize transmitter.

        socket: the ZMQ socket to use if no other is specified on send()/recv()
        calls.

        name: the name to use in the message header.
        """
        self.name: str = name
        self.msgheader: MessageHeader = MessageHeader(name, Protocol.CDTP)
        # if no socket: might just use the transmitter for decoding
        # TODO : refactorize into own class
        self._socket: zmq.Socket | None = socket  # type: ignore[type-arg]
        self.sequence_number: int = 0

    def send_start(
        self, payload: Any, meta: dict[str, Any] | None = None, flags: int = 0
    ) -> None:
        """
        Send starting message of data run over a ZMQ socket.

        Follows the Constellation Data Transmission Protocol.

        payload: meta information about the beginning of run.

        flags: additional ZMQ socket flags to use during transmission.

        """
        self.sequence_number = 0
        packer = msgpack.Packer()
        self._dispatch(
            msgtype=CDTPMessageIdentifier.BOR,
            payload=packer.pack(payload),
            meta=meta,
            flags=flags,
        )

    def send_data(
        self, payload: Any, meta: dict[str, Any] | None = None, flags: int = 0
    ) -> None:
        """
        Send data message of data run over a ZMQ socket.

        Follows the Constellation Data Transmission Protocol.

        payload: meta information about the beginning of run.

        meta: optional dictionary that is sent as a map of string/value
        pairs with the header.

        flags: additional ZMQ socket flags to use during transmission.

        """
        self.sequence_number += 1
        self._dispatch(
            msgtype=CDTPMessageIdentifier.DAT,
            payload=payload,
            meta=meta,
            flags=flags,
        )

    def send_end(
        self, payload: Any, meta: dict[str, Any] | None = None, flags: int = 0
    ) -> None:
        """
        Send ending message of data run over a ZMQ socket.

        Follows the Constellation Data Transmission Protocol.

        payload: meta information about the end of run.

        flags: additional ZMQ socket flags to use during transmission.

        """
        packer = msgpack.Packer()
        self._dispatch(
            msgtype=CDTPMessageIdentifier.EOR,
            payload=packer.pack(payload),
            meta=meta,
            flags=flags,
        )

    def recv(self, flags: int = 0) -> CDTPMessage | None:
        """Receive a multi-part data transmission.

        Follows the Constellation Data Transmission Protocol.

        flags: additional ZMQ socket flags to use during transmission.

        Returns: CTDPMessage

        """
        # check that we have a valid socket
        if not self._socket:
            return None
        try:
            binmsg = self._socket.recv_multipart(flags=flags)
        except zmq.ZMQError:
            return None
        return self.decode(binmsg)

    def decode(self, binmsg: list[bytes]) -> CDTPMessage:
        """Decode a binary message into a CTDPMessage."""
        msg = CDTPMessage()
        msg.set_header(*self.msgheader.decode(binmsg[0]))

        # Retrieve payload
        if msg.msgtype in [CDTPMessageIdentifier.EOR, CDTPMessageIdentifier.BOR]:
            # decode single-frame EOR/BOR payload
            msg.payload = msgpack.unpackb(binmsg[1])
        else:
            # one-or-many binary frames
            msg.payload = binmsg[1:]
            if len(msg.payload) <= 1:
                # unpack list
                try:
                    msg.payload = binmsg[1]
                except IndexError:
                    msg.payload = None
        return msg

    def _dispatch(
        self,
        msgtype: CDTPMessageIdentifier,
        payload: Any = None,
        meta: dict[str, Any] | None = None,
        flags: int = 0,
    ) -> None:
        """Dispatch CDTP message.

        msgtype: flag identifying whether transmitting beginning-of-run, data or end-of-run

        payload: data to send.

        meta: dictionary to include in the map of the message header.

        flags: additional ZMQ socket flags to use during transmission.

        Returns: None

        """
        # check that we have a valid socket
        if not self._socket:
            return

        if payload:
            flags = zmq.SNDMORE | flags
        # message header
        self.msgheader.send(
            self._socket,
            meta=meta,
            flags=flags,
            msgtype=msgtype.value,
            seqno=self.sequence_number,
        )

        # payload
        if payload:
            # send multiple frames?
            if isinstance(payload, list):
                for idx, frame in enumerate(payload):
                    # final package?
                    if idx == len(payload) - 1:
                        flags = flags & (~zmq.SNDMORE)  # flip SNDMORE bit
                    self._socket.send(frame, flags=flags)
            else:
                # single frame
                flags = flags & (~zmq.SNDMORE)  # flip SNDMORE bit
                self._socket.send(payload, flags=flags)
