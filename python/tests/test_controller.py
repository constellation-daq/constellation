#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import pytest
import time
from constellation.core.configuration import flatten_config


# %%%%%%%%%%%%%%%
# TESTS
# %%%%%%%%%%%%%%%


@pytest.mark.forked
def test_controller_instantiation(mock_controller):
    """Test that we can create the controller."""
    assert mock_controller.name == "BaseController.mock_controller"


@pytest.mark.forked
def test_satellite_access_via_array(mock_controller, mock_satellite):
    """Test state cmd reception via satellite array."""
    time.sleep(0.1)
    res = mock_controller.constellation.Satellite.mock_satellite.get_state()
    assert res["msg"] == "new"


@pytest.mark.forked
def test_satellite_hb_state(mock_controller, mock_satellite):
    """Test heartbeat state check."""
    time.sleep(0.1)
    assert mock_controller.states["Satellite.mock_satellite"].name.lower() == "new"


@pytest.mark.forked
def test_satellite_init_w_fullcfg(mock_controller, mock_example_satellite, rawconfig):
    """Test cmd reception."""
    satellite = mock_example_satellite
    time.sleep(0.1)
    res = mock_controller.constellation.MockExampleSatellite.mock_satellite.initialize(
        rawconfig
    )
    assert res["msg"] == "transitioning"
    time.sleep(0.1)
    assert satellite.config._config == flatten_config(
        rawconfig, "MockExampleSatellite", "mock_satellite"
    )
    assert (
        "current_limit" not in satellite.config._config
    ), "Found unexpected item in Satellite cfg after init"
