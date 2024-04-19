#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import pytest
import time
from unittest.mock import MagicMock, patch
import zmq

from constellation.core.cscp import CSCPMessageVerb, CommandTransmitter

from constellation.core.commandmanager import cscp_requestable, CommandReceiver


mock_packet_queue_recv = []
mock_packet_queue_sender = []


# SIDE EFFECTS RECEIVER
def mock_sock_send_recv(payload, flags):
    """Append buf to queue."""
    mock_packet_queue_sender.append(payload)


def mock_sock_recv_multipart_recv(flags):
    """Pop entry from queue."""
    if not mock_packet_queue_recv:
        raise zmq.ZMQError("Resource temporarily unavailable")
    # "pop all"
    r, mock_packet_queue_recv[:] = mock_packet_queue_recv[:], []
    return r


# SIDE EFFECTS SENDER
def mock_sock_send_sender(payload, flags):
    """Append buf to queue."""
    mock_packet_queue_recv.append(payload)


def mock_sock_recv_multipart_sender(flags):
    """Pop entry from queue."""
    if not mock_packet_queue_sender:
        raise zmq.ZMQError("Resource temporarily unavailable")
    # "pop all"
    r, mock_packet_queue_sender[:] = mock_packet_queue_sender[:], []
    return r


# FIXTURES
@pytest.fixture
def mock_socket_recv():
    mock = MagicMock()
    mock = mock.return_value
    mock.send = MagicMock(side_effect=mock_sock_send_recv)
    mock.recv_multipart = MagicMock(side_effect=mock_sock_recv_multipart_recv)
    yield mock


@pytest.fixture
def mock_socket_sender():
    mock = MagicMock()
    mock = mock.return_value
    mock.send = MagicMock(side_effect=mock_sock_send_sender)
    mock.recv_multipart = MagicMock(side_effect=mock_sock_recv_multipart_sender)
    yield mock


@pytest.fixture
def mock_transmitter(mock_socket_sender):
    t = CommandTransmitter("mock_sender", mock_socket_sender)
    yield t


@pytest.fixture
def mock_cmdreceiver(mock_socket_recv):
    class MockCommandReceiver(CommandReceiver):
        @cscp_requestable
        def get_state(self, msg):
            return "state", "good", None

        @cscp_requestable
        def fcnallowed(self, msg):
            return "allowed", "allowed passed", None

        def _fcnallowed_is_allowed(self, msg):
            return True

        @cscp_requestable
        def fcnnotallowed(self, msg):
            return "notallowed", "yes", None

        def _fcnnotallowed_is_allowed(self, msg):
            return False

    with patch("constellation.core.commandmanager.zmq.Context") as mock:
        mock_context = MagicMock()
        mock_context.socket.return_value = mock_socket_recv
        mock.return_value = mock_context
        cr = MockCommandReceiver("mock_satellite", cmd_port=1111, interface="127.0.0.1")
        cr._add_com_thread()
        cr._start_com_threads()
        # give the thread a chance to start
        time.sleep(0.5)
        yield cr


@pytest.mark.forked
def test_cmdtransmitter_send_recv(mock_socket_sender, mock_socket_recv):
    """Test self-concistency between two transmitters (sender/receiver)."""
    sender = CommandTransmitter("mock_sender", mock_socket_sender)
    receiver = CommandTransmitter("mock_receiver", mock_socket_recv)
    # send a request
    sender.send_request("make", "sandwich")
    req = receiver.get_message()
    assert req.msg == "make"
    assert req.payload == "sandwich"
    receiver.send_reply("no", CSCPMessageVerb.INVALID, "make your own sandwich")
    req = sender.get_message()
    assert req.msg == "no"
    assert "make your" in req.payload


@pytest.mark.forked
def test_cmdtransmitter_case_insensitve(mock_socket_sender, mock_socket_recv):
    """Test that commands are received case insensitive (i.e. lower)."""
    sender = CommandTransmitter("mock_sender", mock_socket_sender)
    receiver = CommandTransmitter("mock_receiver", mock_socket_recv)
    # send a request
    sender.send_request("MAKE", "Sandwich")
    req = receiver.get_message()
    assert req.msg == "make"
    assert req.payload == "Sandwich"


@pytest.mark.forked
def test_command_receiver(mock_cmdreceiver, mock_transmitter):
    """Test sending cmds and retrieving answers."""
    # cmd w/o '_is_allowed' method: always allowed
    mock_transmitter.send_request("get_state")
    # give the thread a chance to receive the message
    time.sleep(0.2)
    rep = mock_transmitter.get_message()
    assert rep.msg_verb == CSCPMessageVerb.SUCCESS
    assert rep.payload == "good"

    # cmd w/ '_is_allowed' method: always returns True
    mock_transmitter.send_request("fcnallowed")
    # give the thread a chance to receive the message
    time.sleep(0.2)
    rep = mock_transmitter.get_message()
    assert rep.msg_verb == CSCPMessageVerb.SUCCESS
    assert rep.payload == "allowed passed"

    # cmd w/ '_is_allowed' method: always returns False
    mock_transmitter.send_request("fcnnotallowed")
    # give the thread a chance to receive the message
    time.sleep(0.2)
    rep = mock_transmitter.get_message()
    assert rep.msg_verb == CSCPMessageVerb.INVALID
    assert not rep.payload


@pytest.mark.forked
def test_thread_shutdown(mock_cmdreceiver, mock_transmitter):
    """Test that receiver thread shuts down properly"""
    # shut down the thread
    mock_cmdreceiver._stop_com_threads()
    # send a request
    mock_transmitter.send_request("get_state")
    time.sleep(0.2)
    rep = mock_transmitter.get_message()
    # reply should be missing
    assert not rep
    assert not mock_cmdreceiver._com_thread_evt


@pytest.mark.forked
def test_cmd_unique_commands(mock_cmdreceiver):
    """Test that commands from different classes do not mix."""

    class MockOtherCommandReceiver(CommandReceiver):
        @cscp_requestable
        def get_unique_value(self, msg):
            return 42, None, None

    with patch("constellation.core.commandmanager.zmq.Context"):
        cr = MockOtherCommandReceiver(
            "mock_other_satellite", cmd_port=22222, interface="127.0.0.1"
        )
        msg, cmds, _meta = cr.get_commands()
        # get_state not part of MockOtherCommandReceiver but of MockCommandReceiver
        assert "get_state" not in cmds
