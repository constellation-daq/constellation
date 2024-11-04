#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import pytest
import time
from unittest.mock import MagicMock

from constellation.core.broadcastmanager import CHIRPBroadcaster, chirp_callback

from constellation.core.chirp import CHIRPServiceIdentifier

from conftest import mock_chirp_packet_queue

offer_data_666 = b"CHIRP\x01\x02\xd4fl\x89\x14g7=*b#\xeb4fy\xda\x17\x7f\xd1\xa7t\xc5\xb6/\xd5\xcc$e\x01\x81ir\x04\x02\x9a"

# SIDE EFFECTS


# FIXTURES
@pytest.fixture
def mock_bm(mock_chirp_socket):
    """Create mock BroadcastManager."""
    bm = CHIRPBroadcaster(name="mock_satellite", group="mockstellation", interface="127.0.0.1")
    bm._add_com_thread()
    bm._start_com_threads()
    yield bm


@pytest.fixture
def mock_bm_parent(mock_chirp_socket):
    """Create mock class inheriting from BroadcastManager."""

    class MockBroadcaster(CHIRPBroadcaster):
        callback_triggered = False

        @chirp_callback(CHIRPServiceIdentifier.DATA)
        def service_callback(self, service):
            self.callback_triggered = True

    bm = MockBroadcaster(name="mock_satellite", group="mockstellation", interface="127.0.0.1")
    bm._add_com_thread()
    bm._start_com_threads()
    yield bm


@pytest.fixture
def mock_bm_alt_parent(mock_chirp_socket):
    """Create alternative mock class inheriting from BroadcastManager.

    Does not use callback decorator.

    """

    class MockAltBroadcaster(CHIRPBroadcaster):
        alt_callback_triggered = False

        def alt_service_callback(self, service):
            self.alt_callback_triggered = True

    bm = MockAltBroadcaster(name="mock_satellite", group="mockstellation", interface="127.0.0.1")
    bm._add_com_thread()
    bm._start_com_threads()
    yield bm


@pytest.mark.forked
def test_manager_register(mock_bm):
    """Test registering services."""
    mock_bm.register_offer(CHIRPServiceIdentifier.HEARTBEAT, 50000)
    mock_bm.register_offer(CHIRPServiceIdentifier.CONTROL, 50001)
    mock_bm.broadcast_offers()
    assert len(mock_chirp_packet_queue) == 2

    # broadcast only one of the services
    mock_bm.broadcast_offers(CHIRPServiceIdentifier.CONTROL)
    assert len(mock_chirp_packet_queue) == 3


@pytest.mark.forked
def test_manager_discover(mock_bm):
    """Test discovering services."""
    mock_chirp_packet_queue.append(offer_data_666)
    # thread running in background listening to "socket"
    time.sleep(0.5)
    mock_bm._stop_com_threads()
    assert mock_bm._beacon._sock.seen == 1
    assert len(mock_bm.discovered_services) == 1
    # check late callback queuing
    mock = MagicMock()
    assert mock_bm.task_queue.empty()
    mock_bm.register_request(CHIRPServiceIdentifier.DATA, mock.callback)
    assert not mock_bm.task_queue.empty()
    # no actual callback happens (only queued)
    assert mock.callback.call_count == 0


@pytest.mark.forked
def test_manager_ext_callback_runtime(mock_bm):
    """Test callback when discovering services registered during runtime."""
    # create external callback
    global callback_seen
    callback_seen = False

    def mycallback(service):
        global callback_seen
        callback_seen = True

    assert mock_bm.task_queue.empty()
    mock_bm.register_request(CHIRPServiceIdentifier.DATA, mycallback)
    mock_chirp_packet_queue.append(offer_data_666)
    # thread running in background listening to "socket"
    time.sleep(0.5)
    assert mock_bm._beacon._sock.seen == 1
    assert len(mock_bm.discovered_services) == 1
    # callback queued but not performed (no worker thread)
    assert not mock_bm.task_queue.empty()
    assert not callback_seen
    fcn, arg = mock_bm.task_queue.get()
    fcn(*arg)
    assert callback_seen


@pytest.mark.forked
def test_manager_method_callback_runtime(mock_bm_alt_parent):
    """Test callback when discovering services registered during runtime."""
    assert mock_bm_alt_parent.task_queue.empty()
    mock_bm_alt_parent.register_request(
        CHIRPServiceIdentifier.DATA,
        mock_bm_alt_parent.alt_service_callback,
    )
    mock_chirp_packet_queue.append(offer_data_666)
    # thread running in background listening to "socket"
    time.sleep(0.5)
    assert mock_bm_alt_parent._beacon._sock.seen == 1
    assert len(mock_bm_alt_parent.discovered_services) == 1
    # callback queued but not performed (no worker thread)
    assert not mock_bm_alt_parent.alt_callback_triggered
    assert not mock_bm_alt_parent.task_queue.empty()
    fcn, arg = mock_bm_alt_parent.task_queue.get()
    fcn(*arg)
    assert mock_bm_alt_parent.alt_callback_triggered


@pytest.mark.forked
def test_manager_callback_decorator(mock_bm_parent):
    """Test callback when discovering services registered via decorator."""
    # create callback
    assert mock_bm_parent.task_queue.empty()
    assert len(mock_bm_parent._chirp_callbacks) == 1
    mock_chirp_packet_queue.append(offer_data_666)
    # thread running in background listening to "socket"
    time.sleep(0.5)
    assert mock_bm_parent._beacon._sock.seen == 1
    assert len(mock_bm_parent.discovered_services) == 1
    # callback queued but not performed (no worker thread)
    assert not mock_bm_parent.task_queue.empty()
    assert not mock_bm_parent.callback_triggered
    fcn, arg = mock_bm_parent.task_queue.get()
    fcn(*arg)
    assert mock_bm_parent.callback_triggered
