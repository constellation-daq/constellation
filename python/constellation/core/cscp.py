#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

Module implementing the Constellation Satellite Control Protocol.
"""

from typing import Any, Optional
import io
from enum import Enum
import zmq
import msgpack  # type: ignore[import-untyped]
from .protocol import MessageHeader, Protocol


class CSCPMessageVerb(Enum):
    """Defines the message types of the CSCP.

    Part of the Constellation Satellite Control Protocol, see
    docs/protocols/cscp.md for details.

    """

    REQUEST = 0x0
    SUCCESS = 0x1
    NOTIMPLEMENTED = 0x2
    INCOMPLETE = 0x3
    INVALID = 0x4
    UNKNOWN = 0x5
    ERROR = 0x6


class CSCPMessage:
    """Class holding details of a received CSCP command."""

    def __init__(self) -> None:
        self.msg: str = ""
        self.msg_verb: Optional[CSCPMessageVerb] = None
        self.from_host: str = ""
        self.timestamp: Optional[msgpack.Timestamp] = None
        self.header_meta: Optional[dict[str, Any]] = None
        self.payload: Any = None

    def set_header(
        self,
        from_host: str,
        timestamp: msgpack.Timestamp,
        meta: Optional[dict[str, Any]],
    ) -> None:
        """Sets information retrieved from a message header."""
        self.from_host = from_host
        self.timestamp = timestamp
        self.header_meta = meta

    def __str__(self) -> str:
        """Pretty-print request."""
        s = "Message '{}' from {} received {} at {} {} payload and meta {}."
        return s.format(
            self.msg,
            self.from_host,
            self.msg_verb,
            self.timestamp,
            "with a" if self.payload else "without a",
            self.header_meta,
        )


class CommandTransmitter:
    """Class implementing Constellation Satellite Control Protocol."""

    def __init__(self, name: str, socket: zmq.Socket):  # type: ignore[type-arg]
        self.msgheader = MessageHeader(name, Protocol.CSCP)
        self.socket = socket

    def send_request(
        self, command: str, payload: Any = None, meta: Optional[dict[str, Any]] = None
    ) -> None:
        """Send a command request to a Satellite with an optional payload.

        meta is an optional dictionary that is sent as a map of string/value
        pairs with the header.

        """
        self._dispatch(
            command,
            CSCPMessageVerb.REQUEST,
            payload=payload,
            meta=meta,
            flags=zmq.NOBLOCK,
        )

    def request_get_response(
        self, command: str, payload: Any = None, meta: dict[str, Any] | None = None
    ) -> CSCPMessage:
        """Send a command request to a Satellite and return response.

        meta is an optional dictionary that is sent as a map of string/value
        pairs with the header.

        """
        self._dispatch(
            command,
            CSCPMessageVerb.REQUEST,
            payload=payload,
            meta=meta,
        )
        msg = self.get_message()
        if not msg:
            raise RuntimeError("Failed to get response")
        if not msg.msg_verb == CSCPMessageVerb.SUCCESS:
            raise RuntimeError(msg.msg)
        return msg

    def send_reply(
        self,
        response: str,
        msgtype: CSCPMessageVerb,
        payload: Any = None,
        meta: Optional[dict[str, Any]] = None,
    ) -> None:
        """Send a reply to a previous command with an optional payload.

        meta is an optional dictionary that is sent as a map of string/value
        pairs with the header.

        """
        self._dispatch(response, msgtype, payload, meta=meta, flags=zmq.NOBLOCK)

    def get_message(self, flags: int = 0) -> CSCPMessage | None:
        """Retrieve and return a CSCPMessage.

        Returns None if no request is waiting and flags==zmq.NOBLOCK.

        Raises RuntimeError if message verb is malformed.

        """
        try:
            cmdmsg = self.socket.recv_multipart(flags)
        except zmq.ZMQError as e:
            if "Resource temporarily unavailable" not in e.strerror:
                raise RuntimeError(
                    "CommandTransmitter encountered zmq exception"
                ) from e
            return None
        msg = CSCPMessage()
        msg.set_header(*self.msgheader.decode(cmdmsg[0]))
        # Decode message verb
        unpacker = msgpack.Unpacker()
        unpacker.feed(cmdmsg[1])
        msg.msg_verb = unpacker.unpack()
        msg.msg = unpacker.unpack()
        try:
            msg.msg_verb = CSCPMessageVerb(msg.msg_verb)
        except ValueError:
            raise RuntimeError(
                f"Received invalid request with msg verb: {msg.msg_verb}"
            )
        # convert to lower case:
        msg.msg = msg.msg.lower()
        try:
            unpacker = msgpack.Unpacker()
            unpacker.feed(cmdmsg[2])
            msg.payload = unpacker.unpack()
        except IndexError:
            pass
        return msg

    def _dispatch(
        self,
        msg: str,
        msgtype: CSCPMessageVerb,
        payload: Any = None,
        meta: Optional[dict[str, Any]] = None,
        flags: int = 0,
    ) -> None:
        """Dispatch a message via ZMQ socket."""
        stream = io.BytesIO()
        packer = msgpack.Packer()
        stream.write(packer.pack(msgtype.value))
        stream.write(packer.pack(msg))
        flags = zmq.SNDMORE | flags
        self.msgheader.send(self.socket, meta=meta, flags=flags)
        if payload is None:
            # invert+and: disable SNDMORE bit
            flags = flags & ~zmq.SNDMORE
        self.socket.send(
            stream.getbuffer(),
            flags=flags,
        )
        if payload is not None:
            flags = flags & ~zmq.SNDMORE
            self.socket.send(packer.pack(payload), flags)
