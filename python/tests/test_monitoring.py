"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import pytest
import logging
import time
import os
from unittest.mock import MagicMock, patch

from constellation.core.cmdp import CMDPTransmitter, Metric, MetricsType

from constellation.core.monitoring import (
    ZeroMQSocketLogListener,
    MonitoringSender,
    schedule_metric,
)

from constellation.core.chirp import (
    CHIRPBeaconTransmitter,
    CHIRPServiceIdentifier,
    CHIRPMessageType,
)

from conftest import mock_packet_queue_sender, mocket, send_port


@pytest.fixture
def mock_transmitter_a():
    """Mock Transmitter endpoint A."""
    m = mocket()
    m.port = send_port
    cmdp = CMDPTransmitter("mock_cmdp", m)
    yield cmdp, m


@pytest.fixture
def mock_transmitter_b(mock_socket_sender):
    """Mock Transmitter endpoint B."""
    cmdp = CMDPTransmitter("mock_cmdp", mock_socket_sender)
    yield cmdp


@pytest.fixture
def mock_monitoringsender():
    """Create a mock MonitoringSender instance."""

    class MyStatProducer(MonitoringSender):
        @schedule_metric("Answer", MetricsType.LAST_VALUE, 0.1)
        def get_answer(self):
            """The answer to the Ultimate Question"""
            # self.log.info("Got the answer!")
            return 42

    def mocket_factory(*args, **kwargs):
        m = mocket()
        return m

    with patch("constellation.core.base.zmq.Context") as mock:
        mock_context = MagicMock()
        mock_context.socket = mocket_factory
        mock.return_value = mock_context
        m = MyStatProducer("mock_sender", send_port, interface="127.0.0.1")
        yield m


@pytest.fixture
def monitoringsender():
    """Create a MonitoringSender instance."""

    class MyStatProducer(MonitoringSender):
        @schedule_metric("Answer", MetricsType.LAST_VALUE, 0.1)
        def get_answer(self):
            """The answer to the Ultimate Question"""
            # self.log.info("Got the answer!")
            return 42

    m = MyStatProducer("mock_sender", send_port, interface="*")
    yield m


@pytest.fixture
def mock_listener(mock_transmitter_b):
    """Create a mock log listener instance."""

    mock_handler = MagicMock()
    mock_handler.handle.return_value = None
    mock_handler.emit.return_value = None
    mock_handler.create_lock.return_value = None
    mock_handler.lock = None
    listener = ZeroMQSocketLogListener(mock_transmitter_b, mock_handler)
    yield listener, mock_handler


@pytest.mark.forked
def test_log_transmission(mock_transmitter_a, mock_transmitter_b):
    cmdp, m = mock_transmitter_a
    log = logging.getLogger()
    rec1 = log.makeRecord("name", 10, __name__, 42, "mock log message", None, None)
    cmdp.send_log(rec1)
    # check that we have a packet ready to be read
    assert len(mock_packet_queue_sender[send_port]) == 3
    rec2 = mock_transmitter_b.recv()
    # check that the packet is processed
    assert len(mock_packet_queue_sender[send_port]) == 0
    assert rec2.getMessage() == "mock log message"


@pytest.mark.forked
def test_stat_transmission(mock_transmitter_a, mock_transmitter_b):
    cmdp, m = mock_transmitter_a
    m1 = Metric("mock_val", "Mmocs", MetricsType.LAST_VALUE, 42)
    assert not m1.sender
    cmdp.send(m1)
    # check that we have a packet ready to be read
    assert len(mock_packet_queue_sender[send_port]) == 3
    m2 = mock_transmitter_b.recv()
    # check that the packet is processed
    assert len(mock_packet_queue_sender[send_port]) == 0
    assert m2.name == "MOCK_VAL"
    assert m2.value == 42
    assert m2.sender == "mock_cmdp"
    assert m2.time


@pytest.mark.forked
def test_log_monitoring(mock_listener, mock_monitoringsender):
    listener, stream = mock_listener
    # ROOT logger needs to have a level set
    logger = logging.getLogger()
    logger.setLevel("DEBUG")
    # get a "remote" logger
    lr = logging.getLogger("mock_sender")
    lr.warning("mock warning before start")
    time.sleep(0.2)
    assert len(mock_packet_queue_sender[send_port]) == 3
    listener.start()
    time.sleep(0.1)
    # processed?
    assert len(mock_packet_queue_sender[send_port]) == 0
    assert stream.handle.called
    # check arg to mock call
    assert isinstance(stream.mock_calls[0][1][0], logging.LogRecord)
    lr.info("mock info")
    time.sleep(0.1)
    assert len(mock_packet_queue_sender[send_port]) == 0
    assert len(stream.mock_calls) == 2


@pytest.mark.forked
def test_log_levels(mock_listener, mock_monitoringsender):
    listener, stream = mock_listener
    # ROOT logger needs to have a level set
    logger = logging.getLogger()
    logger.setLevel(1)
    # get a "remote" logger
    lr = logging.getLogger("mock_sender")
    # log a custom level
    lr.trace("mock trace message")
    lr.status("mock status message")
    time.sleep(0.1)
    mock_packet_queue_sender[send_port][0].startswith(b"LOG/TRACE")
    mock_packet_queue_sender[send_port][3].startswith(b"LOG/STATUS")
    # error mapped to critical?
    lr.error("mock critical!!!!")
    time.sleep(0.1)
    mock_packet_queue_sender[send_port][3].startswith(b"LOG/CRITICAL")
    # check reconstruction
    listener.start()
    time.sleep(0.1)
    assert isinstance(stream.mock_calls[0][1][0], logging.LogRecord)
    assert isinstance(stream.mock_calls[1][1][0], logging.LogRecord)
    assert isinstance(stream.mock_calls[2][1][0], logging.LogRecord)
    assert stream.mock_calls[0][1][0].levelname == "TRACE"
    assert stream.mock_calls[1][1][0].levelname == "STATUS"
    assert stream.mock_calls[2][1][0].levelname == "CRITICAL"


@pytest.mark.forked
def test_monitoring_sender_init(mock_listener, mock_monitoringsender):
    m = mock_monitoringsender
    # is our method registered?
    assert len(m._metrics_callbacks) == 1


@pytest.mark.forked
def test_monitoring_sender_loop(mock_listener, mock_monitoringsender):
    m = mock_monitoringsender
    assert send_port not in mock_packet_queue_sender
    # start metric sender thread
    m._add_com_thread()
    m._start_com_threads()
    time.sleep(0.3)
    assert b"STAT/GET_ANSWER" in mock_packet_queue_sender[send_port]


@pytest.mark.forked
def test_monitoring_file_writing(monitoringlistener, monitoringsender):
    ml, tmpdir = monitoringlistener
    ms = monitoringsender
    assert len(ml._log_listeners) == 0
    chirp = CHIRPBeaconTransmitter("mock_sender", "mockstellation", interface="*")
    chirp.broadcast(CHIRPServiceIdentifier.MONITORING, CHIRPMessageType.OFFER, send_port)
    # start metric sender thread
    ms._add_com_thread()
    ms._start_com_threads()
    time.sleep(1)
    assert len(ml._log_listeners) == 1
    assert len(ml._metric_sockets) == 1
    assert os.path.exists(os.path.join(tmpdir, "logs")), "Log output directory not created"
    assert os.path.exists(os.path.join(tmpdir, "stats")), "Stats output directory not created"
    assert os.path.exists(os.path.join(tmpdir, "logs", "mockstellation.log")), "No log file created"
    assert os.path.exists(
        os.path.join(tmpdir, "stats", "MyStatProducer.mock_sender.get_answer.csv")
    ), "Expected output metrics csv not found"
