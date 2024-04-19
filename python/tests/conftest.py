"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import pytest
from unittest.mock import patch, MagicMock
import operator
import threading
import time
import zmq

from constellation.core.satellite import Satellite

from constellation.core.chirp import (
    CHIRP_PORT,
    CHIRPBeaconTransmitter,
)

from constellation.core.cscp import CommandTransmitter
from constellation.core.cdtp import DataTransmitter
from constellation.core.controller import BaseController

# chirp
mock_chirp_packet_queue = []

# satellite
mock_packet_queue_recv = {}
mock_packet_queue_sender = {}
send_port = 11111
recv_port = 22222

SNDMORE_MARK = (
    "_S/END_"  # Arbitrary marker for SNDMORE flag used in mocket packet queues_
)
CHIRP_OFFER_CTRL = b"\x96\xa9CHIRP%x01\x02\xc4\x10\xc3\x941\xda'\x96_K\xa6JU\xac\xbb\xfe\xf1\xac\xc4\x10:\xb9W2E\x01R\xa2\x93|\xddA\x9a%\xb6\x90\x01\xcda\xa9"  # noqa: E501


# SIDE EFFECTS
def mock_chirp_sock_sendto(buf, addr):
    """Append buf to queue."""
    mock_chirp_packet_queue.append(buf)


def mock_chirp_sock_recvfrom(bufsize):
    """Pop entry from queue."""
    try:
        return mock_chirp_packet_queue.pop(0), ["somehost", CHIRP_PORT]
    except IndexError:
        raise BlockingIOError("no mock data")


@pytest.fixture
def mock_chirp_socket():
    """Mock CHIRP socket calls."""
    with patch("constellation.core.chirp.socket.socket") as mock:
        mock = mock.return_value
        mock.connected = MagicMock(return_value=True)
        mock.sendto = MagicMock(side_effect=mock_chirp_sock_sendto)
        mock.recvfrom = MagicMock(side_effect=mock_chirp_sock_recvfrom)
        yield mock


@pytest.fixture
def mock_chirp_transmitter(mock_chirp_socket):
    t = CHIRPBeaconTransmitter("mock_sender", "mockstellation", "127.0.0.1")
    yield t


class mocket(MagicMock):
    """Mock socket for a receiver."""

    def __init__(self, *args, **kwargs):
        super().__init__()
        self.port = 0
        # sender/receiver?
        self.endpoint = 0  # 0 or 1

    def _get_queue(self, out: bool):
        """Flip what queue to use depending on direction and endpoint.

        Makes sure that A sends on B's receiving queue and vice-versa.

        """
        if operator.xor(self.endpoint, out):
            return mock_packet_queue_sender
        else:
            return mock_packet_queue_recv

    def send(self, payload, flags=None):
        """Append buf to queue."""
        try:
            if isinstance(flags, zmq.Flag) and zmq.SNDMORE in flags:
                self._get_queue(True)[self.port].append(payload)
            else:
                self._get_queue(True)[self.port].append([payload, SNDMORE_MARK])
        except KeyError:
            if isinstance(flags, zmq.Flag) and zmq.SNDMORE in flags:
                self._get_queue(True)[self.port] = [payload]
            else:
                self._get_queue(True)[self.port] = [[payload, SNDMORE_MARK]]

    def send_string(self, payload, flags=None):
        self.send(payload.encode(), flags=flags)

    def recv_multipart(self, flags=None):
        """Pop entry from queue."""
        if flags == zmq.NOBLOCK:
            if (
                self.port not in self._get_queue(False)
                or not self._get_queue(False)[self.port]
            ):
                raise zmq.ZMQError("Resource temporarily unavailable")
        else:
            while (
                self.port not in self._get_queue(False)
                or not self._get_queue(False)[self.port]
            ):
                time.sleep(0.01)
        r = []
        RCV_MORE = True
        while RCV_MORE:
            dat = self._get_queue(False)[self.port].pop(0)
            if isinstance(dat, list) and SNDMORE_MARK in dat:
                RCV_MORE = False
                r.append(dat[0])
            else:
                r.append(dat)
        return r

    def recv(self, flags=None):
        """Pop single entry from queue."""
        if flags == zmq.NOBLOCK:
            if (
                self.port not in self._get_queue(False)
                or not self._get_queue(False)[self.port]
            ):
                raise zmq.ZMQError("Resource temporarily unavailable")

            dat = self._get_queue(False)[self.port].pop(0)

            if isinstance(dat, list) and SNDMORE_MARK in dat:
                r = dat[0]
            else:
                r = dat
            return r
        else:
            # block
            while (
                self.port not in self._get_queue(False)
                or not self._get_queue(False)[self.port]
            ):
                time.sleep(0.01)
            dat = self._get_queue(False)[self.port].pop(0)
            if isinstance(dat, list) and SNDMORE_MARK in dat:
                r = dat[0]
            else:
                r = dat
            return r

    def bind(self, host):
        self.port = int(host.split(":")[2])
        print(f"Bound Mocket on {self.port}")

    def connect(self, host):
        self.port = int(host.split(":")[2])
        print(f"Bound Mocket on {self.port}")


@pytest.fixture
def mock_socket_sender():
    mock = mocket()
    mock.return_value = mock
    mock.endpoint = 1
    mock.port = send_port
    yield mock


@pytest.fixture
def mock_socket_receiver():
    mock = mocket()
    mock.return_value = mock
    mock.endpoint = 0
    mock.port = send_port
    yield mock


@pytest.fixture
def mock_cmd_transmitter(mock_socket_sender):
    t = CommandTransmitter("mock_sender", mock_socket_sender)
    yield t


@pytest.fixture
def mock_data_transmitter(mock_socket_sender):
    t = DataTransmitter("mock_sender", mock_socket_sender)
    yield t


@pytest.fixture
def mock_data_receiver(mock_socket_receiver):
    r = DataTransmitter("mock_receiver", mock_socket_receiver)
    yield r


@pytest.fixture
def mock_satellite(mock_chirp_socket):
    """Create a mock Satellite base instance."""

    def mocket_factory(*args, **kwargs):
        m = mocket()
        return m

    with patch("constellation.core.base.zmq.Context") as mock:
        mock_context = MagicMock()
        mock_context.socket = mocket_factory
        mock.return_value = mock_context
        s = Satellite(
            "mock_satellite", "mockstellation", 11111, 22222, 33333, "127.0.0.1"
        )
        t = threading.Thread(target=s.run_satellite)
        t.start()
        # give the threads a chance to start
        time.sleep(0.1)
        yield s


@pytest.fixture
def mock_controller(mock_chirp_socket):
    """Create a mock Controller base instance."""

    def mocket_factory(*args, **kwargs):
        m = mocket()
        m.endpoint = 1
        return m

    with patch("constellation.core.base.zmq.Context") as mock:
        mock_context = MagicMock()
        mock_context.socket = mocket_factory
        mock.return_value = mock_context
        c = BaseController("mock_controller", "mockstellation", "127.0.0.1")
        # give the threads a chance to start
        time.sleep(0.1)
        yield c
