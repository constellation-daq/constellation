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

    def send(self, payload, meta: dict = None, socket: zmq.Socket = None, flags: int = 0):
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
        socket.send(msgpack.packb(PROTOCOL_IDENTIFIER), flags=flags)
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
            raise RuntimeError(f"Received message with wrong length of {len(msg)} parts!")
        if not msgpack.unpackb(msg[0]) == PROTOCOL_IDENTIFIER:
            raise RuntimeError(f"Received message with malformed CDTP header: {msgpack.unpackb(msg[0])}!")
        payload = msgpack.unpackb(msg[4])
        meta = msgpack.unpackb(msg[3])
        ts = msgpack.unpackb(msg[2])
        host = msgpack.unpackb(msg[1])
        return payload, meta, host, ts

    def close(self):
        """Close the socket."""
        self._socket.close()


class LogTransmitter:
    """Class for sending Constellation log messages via ZMQ."""

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

    def send(self, record: logging.logRecord, socket: zmq.Socket = None):
        """Send a logRecord via an ZMQ socket.

        Follows the Constellation Monitoring Distribution Protocol.

        record: logRecord to send.

        socket: ZMQ socket to use for transmission. If none is specified, use
        the one the class was initialized with.

        Returns: return of socket.send(record) call.

        """
        # use default socket if none was specified
        if not socket:
            socket = self._socket
        topic = f'LOG/{record.levelname}/{record.name}'
        header = [PROTOCOL_IDENTIFIER, self.host, msgpack.Timestamp.from_unix_nano(time.time_ns()), {}]
        socket.send_string(topic, zmq.SNDMORE)
        socket.send(msgpack.packb(header), zmq.SNDMORE)
        # Instead of just adding the formatted message, this adds key attributes
        # of the logRecord, allowing to reconstruct the full message on the
        # other end.
        # TODO filter and name these according to the Constellation LOG specifications
        return socket.send(msgpack.packb(
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

    def recv(self, socket: zmq.Socket = None):
        """Receive a Constellation log message and return a logRecord.

        Follows the Constellation Monitoring Distribution Protocol.

        socket: ZMQ socket to use for transmission. If none is specified, use
        the one the class was initialized with.

        Returns: logRecord.

        """
        # use default socket if none was specified
        if not socket:
            socket = self._socket
        topic = msgpack.unpackb(self.queue.recv())
        # NOTE it is currently not possible to receive both STATS and LOG on the
        # same socket as the respective classes do not handle the other.
        # TODO : refactorize LogTransmitter to allow to transparently receive logs and stats
        if not topic.startswith("LOG/"):
            raise RuntimeError(f"LogTransmitter cannot decode messages of topic '{topic}'")
        header = msgpack.unpackb(self.queue.recv())
        if not header[0] == PROTOCOL_IDENTIFIER:
            raise RuntimeError(f"Received message with malformed CDTP header: {header}!")
        record = msgpack.unpackb(self.queue.recv())
        return logging.makeLogRecord(record)

    def close(self):
        """Close the socket."""
        self._socket.close()


class MetricsType(Enum):
    LAST_VALUE = 0x1
    ACCUMULATE = 0x2
    AVERAGE = 0x3
    RATE = 0x4


class Metric:
    """Class to hold information for a Constellation metric."""

    def __init__(self, name: str, description: str, unit: str, handling:
                 MetricsType, value=None):
        self.name = name
        self.description = description
        self.unit = unit
        self.handling = handling
        self.value = value


class MetricsTransmitter:
    """Class to send and receive a Constellation Metric."""

    def __init__(self, socket: zmq.Socket = None, host: str = None) -> None:
        """Initialize transmitter.

        socket: the ZMQ socket to use if no other is specified on send()/recv()
        calls.

        host: the name to use in the message header. Defaults to system host name.

        """
        if not host:
            host = platform.node()
        self.host = host
        self._socket = socket

    def send(self, metric: Metric, socket: zmq.Socket = None):
        """Send a metric via a ZMQ socket.

        Follows the Constellation Monitoring Distribution Protocol.

        metric: Metric to send.

        socket: ZMQ socket to use for transmission. If none is specified, use
        the one the class was initialized with.

        Returns: return of socket.send(metric) call.

        """
        # use default socket if none was specified
        if not socket:
            socket = self._socket
        topic = "STATS/" + metric.name
        header = [PROTOCOL_IDENTIFIER, self.host,
                  msgpack.Timestamp.from_unix_nano(time.time_ns()), {}]
        self._socket.send_string(topic, zmq.SNDMORE)
        self.queue.send(msgpack.packb(header), zmq.SNDMORE)
        self.queue.send(msgpack.packb([metric.description, metric.unit,
                                       metric.handling.value, metric.value]))

    def recv(self, socket: zmq.Socket = None):
        """Receive a Constellation STATS message and return a Metric.

        Follows the Constellation Monitoring Distribution Protocol.

        socket: ZMQ socket to use for transmission. If none is specified, use
        the one the class was initialized with.

        Returns: Metric.

        """
        # use default socket if none was specified
        if not socket:
            socket = self._socket
        topic = msgpack.unpackb(self.queue.recv())
        # NOTE it is currently not possible to receive both STATS and LOG on the
        # same socket as the respective classes do not handle the other.
        # TODO : refactorize MetricsTransmitter to allow to transparently receive logs and stats
        if not topic.startswith("STATS/"):
            raise RuntimeError(f"MetricsTransmitter cannot decode messages of topic '{topic}'")
        name = topic.split('/')[1]
        header = msgpack.unpackb(self.queue.recv())
        if not header[0] == PROTOCOL_IDENTIFIER:
            raise RuntimeError(f"Received message with malformed CDTP header: '{header}'!")
        description, unit, handling, value = msgpack.unpackb(self.queue.recv())
        return Metric(name, description, unit, MetricsType(handling), value)

    def close(self):
        """Close the socket."""
        self._socket.close()


class SatelliteResponse(Enum):
    """Defines the response codes of a Satellite.

    Part of the Constellation Command Protocol.

    """
    SUCCESS = 0
    INVALID = 1
    NOTIMPLEMENTED = 2
    INCOMPLETE = 3
