#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import pytest
import time
import threading
from unittest.mock import MagicMock, patch

from constellation.core.cscp import CSCPMessageVerb
from constellation.satellites.Mariner.Mariner import Mariner

from conftest import mocket, wait_for_state


@pytest.fixture
def mariner_satellite(mock_chirp_transmitter):
    """Mock a Satellite for a specific device, ie. a class inheriting from Satellite."""

    def mocket_factory(*args, **kwargs):
        m = mocket()
        return m

    with patch("constellation.core.base.zmq.Context") as mock:
        mock_context = MagicMock()
        mock_context.socket = mocket_factory
        mock.return_value = mock_context
        s = Mariner("Nine", "mockstellation", 11111, 22222, 33333, "127.0.0.1")
        t = threading.Thread(target=s.run_satellite)
        t.start()
        # give the threads a chance to start
        time.sleep(0.1)
        yield s


# %%%%%%%%%%%%%%%
# TESTS
# %%%%%%%%%%%%%%%


@pytest.mark.forked
def test_mariner_instantiation(mariner_satellite):
    """Test that we can create the satellite."""
    assert mariner_satellite.name == "Mariner.Nine"


@pytest.mark.forked
def test_mariner_fsm_transition(mock_cmd_transmitter, mariner_satellite):
    """Test that Satellite can 'walk' through a series of transitions."""
    transitions = {
        "initialize": "INIT",
        "launch": "ORBIT",
        "start": "RUN",
        "stop": "ORBIT",
        "land": "INIT",
    }
    sender = mock_cmd_transmitter
    for cmd, state in transitions.items():
        if cmd == "initialize":
            payload = {"voltage": 1000, "current": 3, "sample_period": 1}
        elif cmd == "start":
            payload = "5001"
        else:
            # send a dict, why not?
            payload = {"mock key": "mock argument string"}
        sender.send_request(cmd, payload)
        time.sleep(0.2)
        req = sender.get_message()
        assert "transitioning" in req.msg.lower()
        assert req.msg_verb == CSCPMessageVerb.SUCCESS
        # wait for state transition

        wait_for_state(mariner_satellite.fsm, state, 4.0)
        # check state
        sender.send_request("get_state")
        time.sleep(0.2)
        req = sender.get_message()
        assert state.lower() in req.msg.lower()
        assert req.msg_verb == CSCPMessageVerb.SUCCESS


@pytest.mark.forked
def test_mariner_cfg_missing_items(mock_cmd_transmitter, mariner_satellite):
    """Test that Satellite can gracefully handle missing cfg items."""
    sender = mock_cmd_transmitter
    payload = {"mock_cfg_key": "mock config string"}
    sender.send_request("initialize", payload)
    time.sleep(0.2)
    req = sender.get_message()
    assert "transitioning" in req.msg.lower()
    assert req.msg_verb == CSCPMessageVerb.SUCCESS
    # wait for state transition

    wait_for_state(mariner_satellite.fsm, "ERROR", 4.0)
    assert "missing a required configuration value" in mariner_satellite.fsm.status


@pytest.mark.forked
def test_mariner_command(mock_cmd_transmitter, mariner_satellite):
    """Test that Satellite can gracefully handle missing cfg items."""
    sender = mock_cmd_transmitter
    sender.send_request("get_attitude", None)
    time.sleep(0.2)
    req = sender.get_message()
    assert "not ready" in req.msg.lower()
    assert req.msg_verb == CSCPMessageVerb.SUCCESS
    assert not req.payload

    payload = {"voltage": 1000, "current": 3, "sample_period": 1}
    sender.request_get_response("initialize", payload)
    time.sleep(0.2)

    sender.send_request("get_attitude", None)
    time.sleep(0.2)
    req = sender.get_message()
    assert "locked and ready" in req.msg.lower()
    assert req.msg_verb == CSCPMessageVerb.SUCCESS
    assert isinstance(req.payload, int)
