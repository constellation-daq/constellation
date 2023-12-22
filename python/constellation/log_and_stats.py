import time
from enum import Enum
import logging

import zmq
import msgpack

from logging.handlers import QueueHandler, QueueListener
from .protocol import LogTransmitter, MetricsTransmitter

class MsgHeader:
    def __init__(self, sender, timestamp: msgpack.Timestamp, tags: dict):

        self.sender = sender
        self.time = timestamp
        self.tags = tags

    def time_ns(self): return self.time.to_unix_nano()

    def time_s(self): return self.time.to_unix()

    def encode(_obj):
        return [_obj.sender, _obj.time, _obj.tags]

    def decode(_data):
        return MsgHeader(_data[0], _data[1], _data[2])


class LogLevels(Enum):
    DEBUG = logging.DEBUG
    INFO = logging.INFO
    WARNING = logging.WARNING
    ERROR = logging.ERROR


def getLoggerAndStats(name: str, context: zmq.Context, port: int):
    """Set up and return a logger and statistics object."""
    # Create socket and bind wildcard
    socket = context.socket(zmq.PUB)
    socket.bind(f'tcp://*:{port}')
    logger = logging.getLogger(name)
    zmqhandler = ZeroMQSocketHandler(socket)
    # set lowest level to ensure that all messages are published
    zmqhandler.setLevel(0)
    logger.addHandler(zmqhandler)
    stats = MetricsTransmitter(socket, name)
    return logger, stats


class ZeroMQSocketHandler(QueueHandler):
    """This handler sends records to a ZMQ socket."""

    def __init__(self, socket: zmq.Socket):
        super().__init__(LogTransmitter(socket))

    def enqueue(self, record):
        self.queue.send(record)

    def close(self):
        if not self.queue.closed:
            self.queue.close()


class ZeroMQSocketListener(QueueListener):
    def __init__(self, uri, /, *handlers, **kwargs):
        self.ctx = kwargs.get('ctx') or zmq.Context()
        socket = zmq.Socket(self.ctx, zmq.SUB)
        # TODO implement a filter parameter to customize what to subscribe to
        socket.setsockopt_string(zmq.SUBSCRIBE, 'LOG/')  # subscribe to LOGs
        socket.connect(uri)
        kwargs.pop('ctx', None)
        super().__init__(LogTransmitter(socket), *handlers, **kwargs)

    def dequeue(self, block):
        return self.queue.recv()


def main(args=None):
    """Start a simple log listener service."""
    import argparse

    parser = argparse.ArgumentParser(description=main.__doc__)
    parser.add_argument("--log-level", default="info")
    parser.add_argument("host", type=str)
    parser.add_argument("port", type=int)
    args = parser.parse_args(args)
    logger = logging.getLogger(__name__)
    formatter = logging.Formatter('%(asctime)s | %(name)s |  %(levelname)s: %(message)s')
    stream_handler = logging.StreamHandler()
    stream_handler.setLevel(args.log_level.upper())
    stream_handler.setFormatter(formatter)
    logger.addHandler(stream_handler)

    ctx = zmq.Context()
    zmqlistener = ZeroMQSocketListener(f'tcp://{args.host}:{args.port}', stream_handler, ctx=ctx)
    zmqlistener.start()
    while True:
        time.sleep(0.01)


if __name__ == "__main__":
    main()
