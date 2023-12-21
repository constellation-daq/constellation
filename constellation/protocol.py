#!/usr/bin/env python3
"""Module implementing the Constellation communication protocols."""

import msgpack
import zmq
import time
import platform
import logging
from enum import Enum


PROTOCOL_IDENTIFIER = "CDTP%x01"


class DataTransmitter:
    """Base class for sending Constellation data packets via ZMQ."""

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


class LogTransmitter:
    """Class for sending Constellation log messages via ZMQ."""

    def __init__(self, host: str = None):
        if not host:
            host = platform.node()
        self.host = host

    def send(self, socket: zmq.Socket, record: logging.logRecord):
        """Send a logRecord via an ZMQ socket."""
        topic = f'LOG/{record.levelname}/{record.name}'
        header = [PROTOCOL_IDENTIFIER, self.host, msgpack.Timestamp.from_unix_nano(time.time_ns()), {}]
        socket.send_string(topic, zmq.SNDMORE)
        self.queue.send(msgpack.packb(header), zmq.SNDMORE)
        # Instead of just adding the formatted message, this adds key attributes
        # of the logRecord, allowing to reconstruct the full message on the
        # other end.
        # TODO filter and name these according to the Constellation LOG specifications
        self.queue.send(msgpack.packb(
            {'name': record.name,
             'msg': record.msg,
             'args': record.args,
             'levelname': record.levelname,
             'levelno': record.levelno,
             'pathname': record.pathname,
             'filename': record.filename,
             'module': record.module,
             'exc_info': record.exc_info,
             'exc_text': record.exc_text,
             'stack_info': record.stack_info,
             'lineno': record.lineno,
             'funcName': record.funcName,
             'created': record.created,
             'msecs': record.msecs,
             'relativeCreated': record.relativeCreated,
             'thread': record.thread,
             'threadName': record.threadName,
             'processName': record.processName,
             'process': record.process,
             'message': record.message,
             }
        ))

    def recv(self, socket: zmq.Socket):
        """Receive a Constellation log message and return a logRecord."""
        topic = msgpack.unpackb(self.queue.recv())
        if not topic.startswith("LOG/"):
            raise RuntimeError(f"LogTransmitter cannot decode messages of topic '{topic}'")
        header = msgpack.unpackb(self.queue.recv())
        if not header[0] == PROTOCOL_IDENTIFIER:
            raise RuntimeError(f"Received message with malformed CDTP header: {header}!")
        record = msgpack.unpackb(self.queue.recv())
        return logging.makeLogRecord(record)


class SatelliteResponse(Enum):
    """Defines the response codes of a Satellite.

    Part of the Constellation Command Protocoll.

    """
    SUCCESS = 0
    INVALID = 1
    NOTIMPLEMENTED = 2
    INCOMPLETE = 3
