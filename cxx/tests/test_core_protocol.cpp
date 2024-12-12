/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <string>

#include <catch2/catch_test_macros.hpp>

#include "constellation/core/protocol/CSCP_definitions.hpp"

using namespace constellation::protocol;

TEST_CASE("States", "[core]") {
    REQUIRE(CSCP::is_steady(CSCP::State::INIT));
    REQUIRE_FALSE(CSCP::is_steady(CSCP::State::launching));

    REQUIRE(CSCP::is_shutdown_allowed(CSCP::State::INIT));
    REQUIRE_FALSE(CSCP::is_shutdown_allowed(CSCP::State::RUN));

    REQUIRE(CSCP::is_one_of_states<CSCP::State::INIT, CSCP::State::RUN>(CSCP::State::INIT));
    REQUIRE_FALSE(CSCP::is_one_of_states<CSCP::State::INIT, CSCP::State::RUN>(CSCP::State::ORBIT));

    REQUIRE_FALSE(CSCP::is_not_one_of_states<CSCP::State::INIT, CSCP::State::RUN>(CSCP::State::INIT));
    REQUIRE(CSCP::is_not_one_of_states<CSCP::State::INIT, CSCP::State::RUN>(CSCP::State::ORBIT));
}

TEST_CASE("Names", "[core]") {
    REQUIRE(CSCP::is_valid_satellite_name("sat_name"));
    REQUIRE_FALSE(CSCP::is_valid_satellite_name("sat-name"));

    REQUIRE(CSCP::is_valid_run_id("run-id_id"));
    REQUIRE_FALSE(CSCP::is_valid_run_id("run-id_*id"));

    REQUIRE(CSCP::is_valid_command_name("my_command4"));
    REQUIRE_FALSE(CSCP::is_valid_command_name("0my_command4"));
}
