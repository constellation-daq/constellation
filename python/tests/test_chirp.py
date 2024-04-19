#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import pytest
from unittest.mock import patch

from constellation.core.chirp import (
    CHIRPBeaconTransmitter,
    CHIRPServiceIdentifier,
    CHIRPMessageType,
)


@pytest.mark.forked
def test_chirp_beacon_send_recv(mock_chirp_socket):
    """Test interplay between two transmitters (sender/receiver)."""
    sender = CHIRPBeaconTransmitter("mock_sender", "mockstellation", "127.0.0.1")
    receiver = CHIRPBeaconTransmitter("mock_receiver", "mockstellation", "127.0.0.1")
    sender.broadcast(CHIRPServiceIdentifier.DATA, CHIRPMessageType.OFFER, 666)
    res = receiver.listen()
    assert (
        res.serviceid == CHIRPServiceIdentifier.DATA
    ), "Receiving chirp package failed."

    # listen a second time
    res = receiver.listen()
    assert not res, "Non-blocking receive w/o sending data failed."

    # receive two packages
    sender.broadcast(CHIRPServiceIdentifier.DATA, CHIRPMessageType.OFFER, 666)
    sender.broadcast(CHIRPServiceIdentifier.DATA, CHIRPMessageType.OFFER, 666)
    res = receiver.listen()
    assert (
        res.serviceid == CHIRPServiceIdentifier.DATA
    ), "First of two packages went missing"
    res = receiver.listen()
    assert (
        res.serviceid == CHIRPServiceIdentifier.DATA
    ), "Second of two packages went missing"
    res = receiver.listen()
    assert not res, "Non-blocking receive w/o sending data failed."

    # send package, then let the test for header fail
    sender.broadcast(CHIRPServiceIdentifier.DATA, CHIRPMessageType.OFFER, 666)
    with patch("constellation.core.chirp.CHIRP_HEADER"):
        with pytest.raises(RuntimeError) as e:
            res = receiver.listen()
        assert "malformed message" in str(
            e.value
        ), "Wrong chirp header did not trigger expected exception message."


@pytest.mark.forked
def test_filter_same_host(mock_chirp_transmitter):
    """Check that same-host packets are dropped."""
    t = mock_chirp_transmitter
    t.broadcast(CHIRPServiceIdentifier.DATA, CHIRPMessageType.OFFER, 666)
    res = t.listen()
    assert not res, "Received packet despite same-host filter."
