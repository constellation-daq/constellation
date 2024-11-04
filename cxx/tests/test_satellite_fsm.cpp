/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <atomic>
#include <chrono> // IWYU pragma: keep
#include <string>
#include <thread>
#include <utility>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <msgpack.hpp>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/FSM.hpp"

#include "dummy_satellite.hpp"

using namespace Catch::Matchers;
using namespace constellation::config;
using namespace constellation::protocol::CSCP;
using namespace constellation::satellite;
using namespace constellation::utils;
using namespace std::chrono_literals;

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Regular FSM operation", "[satellite][satellite::fsm]") {
    DummySatellite satellite {};
    auto& fsm = satellite.getFSM();

    // NEW -> INIT
    fsm.react(Transition::initialize, Configuration());
    REQUIRE(fsm.getState() == State::initializing);
    satellite.progressFsm();
    REQUIRE(fsm.getState() == State::INIT);
    // INIT -> INIT
    fsm.react(Transition::initialize, Configuration());
    REQUIRE(fsm.getState() == State::initializing);
    satellite.progressFsm();
    REQUIRE(fsm.getState() == State::INIT);
    // INIT -> ORBIT
    fsm.react(Transition::launch);
    REQUIRE(fsm.getState() == State::launching);
    satellite.progressFsm();
    REQUIRE(fsm.getState() == State::ORBIT);
    // ORBIT -> ORBIT
    fsm.react(Transition::reconfigure, Configuration());
    REQUIRE(fsm.getState() == State::reconfiguring);
    satellite.progressFsm();
    REQUIRE(fsm.getState() == State::ORBIT);
    // ORBIT -> RUN
    fsm.react(Transition::start, "run_0");
    REQUIRE(fsm.getState() == State::starting);
    satellite.progressFsm();
    REQUIRE(fsm.getState() == State::RUN);
    // RUN -> ORBIT
    fsm.react(Transition::stop);
    REQUIRE(fsm.getState() == State::stopping);
    satellite.progressFsm();
    REQUIRE(fsm.getState() == State::ORBIT);
    // ORBIT -> INT
    fsm.react(Transition::land);
    REQUIRE(fsm.getState() == State::landing);
    satellite.progressFsm();
    REQUIRE(fsm.getState() == State::INIT);
}

TEST_CASE("FSM interrupts and failures", "[satellite][satellite::fsm]") {
    DummySatellite satellite {};
    auto& fsm = satellite.getFSM();

    // Failure in transitional state
    fsm.react(Transition::initialize, Configuration());
    REQUIRE(fsm.getState() == State::initializing);
    satellite.setThrowTransitional();
    while(fsm.getState() == State::initializing) {
        std::this_thread::sleep_for(10ms);
    }
    REQUIRE(fsm.getState() == State::ERROR);

    // Failure on failure not allowed (use reactIfAllowed)
    REQUIRE_FALSE(fsm.isAllowed(Transition::failure));
    REQUIRE_FALSE(fsm.reactIfAllowed(Transition::failure));

    // Reset
    fsm.react(Transition::initialize, Configuration());
    satellite.progressFsm();
    REQUIRE(fsm.getState() == State::INIT);

    // Interrupt in RUN state
    fsm.react(Transition::launch);
    satellite.progressFsm();
    REQUIRE(fsm.getState() == State::ORBIT);
    fsm.react(Transition::start, "run_0");
    satellite.progressFsm();
    REQUIRE(fsm.getState() == State::RUN);
    fsm.react(Transition::interrupt);
    std::this_thread::sleep_for(150ms); // Give some time call stopping and landing
    satellite.progressFsm();
    REQUIRE(fsm.getState() == State::SAFE);
}

TEST_CASE("React via CSCP", "[satellite][satellite::fsm][cscp]") {
    DummySatellite satellite {};
    auto& fsm = satellite.getFSM();
    using constellation::message::CSCP1Message;

    auto payload_frame = Dictionary().assemble();
    auto ret = std::pair<constellation::message::CSCP1Message::Type, std::string>();

    // Initialize requires frame
    ret = fsm.reactCommand(TransitionCommand::initialize, {});
    REQUIRE(ret.first == CSCP1Message::Type::INCOMPLETE);
    REQUIRE_THAT(ret.second, Equals("Transition initialize requires a payload frame"));
    ret = fsm.reactCommand(TransitionCommand::initialize, payload_frame);
    REQUIRE(ret.first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(ret.second, Equals("Transition initialize is being initiated"));

    // INVALID when not allowed
    satellite.progressFsm();
    ret = fsm.reactCommand(TransitionCommand::start, {});
    REQUIRE(ret.first == CSCP1Message::Type::INVALID);
    REQUIRE_THAT(ret.second, Equals("Transition start not allowed from INIT state"));

    // payload is ignored when not used
    ret = fsm.reactCommand(TransitionCommand::launch, payload_frame);
    REQUIRE(ret.first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(ret.second, Equals("Transition launch is being initiated (payload frame is ignored)"));
    satellite.progressFsm();

    // INVALID when invalid run ID is provided
    msgpack::sbuffer sbuf {};
    msgpack::pack(sbuf, "run_12&34");
    const auto payload_string = constellation::message::PayloadBuffer(std::move(sbuf));
    ret = fsm.reactCommand(TransitionCommand::start, payload_string);
    REQUIRE(ret.first == CSCP1Message::Type::INCOMPLETE);
    REQUIRE_THAT(ret.second,
                 Equals("Transition start received invalid payload: Run identifier contains invalid characters"));

    // NOTIMPLEMENTED if reconfigure not supported
    satellite.setSupportReconfigure(false);
    ret = fsm.reactCommand(TransitionCommand::reconfigure, payload_frame);
    REQUIRE(ret.first == CSCP1Message::Type::NOTIMPLEMENTED);
    REQUIRE_THAT(ret.second, Equals("Transition reconfigure is not implemented by this satellite"));
}

// NOLINTNEXTLINE(readability-function-size)
TEST_CASE("Allowed FSM transitions", "[satellite][satellite::fsm]") {
    DummySatellite satellite {};
    auto& fsm = satellite.getFSM();
    using enum Transition;

    REQUIRE(fsm.getState() == State::NEW);
    // Allowed in NEW: initialize, failure
    REQUIRE_THROWS_WITH(fsm.react(initialized), Equals("Transition initialized not allowed from NEW state"));
    REQUIRE_THROWS_WITH(fsm.react(launch), Equals("Transition launch not allowed from NEW state"));
    REQUIRE_THROWS_WITH(fsm.react(launched), Equals("Transition launched not allowed from NEW state"));
    REQUIRE_THROWS_WITH(fsm.react(land), Equals("Transition land not allowed from NEW state"));
    REQUIRE_THROWS_WITH(fsm.react(landed), Equals("Transition landed not allowed from NEW state"));
    REQUIRE_THROWS_WITH(fsm.react(reconfigure), Equals("Transition reconfigure not allowed from NEW state"));
    REQUIRE_THROWS_WITH(fsm.react(reconfigured), Equals("Transition reconfigured not allowed from NEW state"));
    REQUIRE_THROWS_WITH(fsm.react(start), Equals("Transition start not allowed from NEW state"));
    REQUIRE_THROWS_WITH(fsm.react(started), Equals("Transition started not allowed from NEW state"));
    REQUIRE_THROWS_WITH(fsm.react(stop), Equals("Transition stop not allowed from NEW state"));
    REQUIRE_THROWS_WITH(fsm.react(stopped), Equals("Transition stopped not allowed from NEW state"));
    REQUIRE_THROWS_WITH(fsm.react(interrupt), Equals("Transition interrupt not allowed from NEW state"));
    REQUIRE_THROWS_WITH(fsm.react(interrupted), Equals("Transition interrupted not allowed from NEW state"));
    INFO("NEW succeeded");

    fsm.react(initialize, Configuration());
    REQUIRE(fsm.getState() == State::initializing);
    std::this_thread::sleep_for(5ms); // Give some to log in the correct order
    // Allowed in initializing: initialized, failure
    REQUIRE_THROWS_WITH(fsm.react(initialize), Equals("Transition initialize not allowed from initializing state"));
    REQUIRE_THROWS_WITH(fsm.react(launch), Equals("Transition launch not allowed from initializing state"));
    REQUIRE_THROWS_WITH(fsm.react(launched), Equals("Transition launched not allowed from initializing state"));
    REQUIRE_THROWS_WITH(fsm.react(land), Equals("Transition land not allowed from initializing state"));
    REQUIRE_THROWS_WITH(fsm.react(landed), Equals("Transition landed not allowed from initializing state"));
    REQUIRE_THROWS_WITH(fsm.react(reconfigure), Equals("Transition reconfigure not allowed from initializing state"));
    REQUIRE_THROWS_WITH(fsm.react(reconfigured), Equals("Transition reconfigured not allowed from initializing state"));
    REQUIRE_THROWS_WITH(fsm.react(start), Equals("Transition start not allowed from initializing state"));
    REQUIRE_THROWS_WITH(fsm.react(started), Equals("Transition started not allowed from initializing state"));
    REQUIRE_THROWS_WITH(fsm.react(stop), Equals("Transition stop not allowed from initializing state"));
    REQUIRE_THROWS_WITH(fsm.react(stopped), Equals("Transition stopped not allowed from initializing state"));
    REQUIRE_THROWS_WITH(fsm.react(interrupt), Equals("Transition interrupt not allowed from initializing state"));
    REQUIRE_THROWS_WITH(fsm.react(interrupted), Equals("Transition interrupted not allowed from initializing state"));
    INFO("initializing succeeded");

    satellite.progressFsm();
    REQUIRE(fsm.getState() == State::INIT);
    std::this_thread::sleep_for(5ms); // Give some to log in the correct order
    // Allowed in INIT: initialize, launch, failure
    REQUIRE(fsm.isAllowed(initialize));
    REQUIRE_THROWS_WITH(fsm.react(initialized), Equals("Transition initialized not allowed from INIT state"));
    REQUIRE(fsm.isAllowed(launch));
    REQUIRE_THROWS_WITH(fsm.react(launched), Equals("Transition launched not allowed from INIT state"));
    REQUIRE_THROWS_WITH(fsm.react(land), Equals("Transition land not allowed from INIT state"));
    REQUIRE_THROWS_WITH(fsm.react(landed), Equals("Transition landed not allowed from INIT state"));
    REQUIRE_THROWS_WITH(fsm.react(reconfigure), Equals("Transition reconfigure not allowed from INIT state"));
    REQUIRE_THROWS_WITH(fsm.react(reconfigured), Equals("Transition reconfigured not allowed from INIT state"));
    REQUIRE_THROWS_WITH(fsm.react(start), Equals("Transition start not allowed from INIT state"));
    REQUIRE_THROWS_WITH(fsm.react(started), Equals("Transition started not allowed from INIT state"));
    REQUIRE_THROWS_WITH(fsm.react(stop), Equals("Transition stop not allowed from INIT state"));
    REQUIRE_THROWS_WITH(fsm.react(stopped), Equals("Transition stopped not allowed from INIT state"));
    REQUIRE_THROWS_WITH(fsm.react(interrupt), Equals("Transition interrupt not allowed from INIT state"));
    REQUIRE_THROWS_WITH(fsm.react(interrupted), Equals("Transition interrupted not allowed from INIT state"));
    REQUIRE(fsm.isAllowed(failure));
    INFO("INIT succeeded");

    fsm.react(launch);
    REQUIRE(fsm.getState() == State::launching);
    std::this_thread::sleep_for(5ms); // Give some to log in the correct order
    // Allowed in launching: launched, interrupt, failure
    REQUIRE_THROWS_WITH(fsm.react(initialize), Equals("Transition initialize not allowed from launching state"));
    REQUIRE_THROWS_WITH(fsm.react(initialized), Equals("Transition initialized not allowed from launching state"));
    REQUIRE_THROWS_WITH(fsm.react(launch), Equals("Transition launch not allowed from launching state"));
    REQUIRE(fsm.isAllowed(launched));
    REQUIRE_THROWS_WITH(fsm.react(land), Equals("Transition land not allowed from launching state"));
    REQUIRE_THROWS_WITH(fsm.react(landed), Equals("Transition landed not allowed from launching state"));
    REQUIRE_THROWS_WITH(fsm.react(reconfigure), Equals("Transition reconfigure not allowed from launching state"));
    REQUIRE_THROWS_WITH(fsm.react(reconfigured), Equals("Transition reconfigured not allowed from launching state"));
    REQUIRE_THROWS_WITH(fsm.react(start), Equals("Transition start not allowed from launching state"));
    REQUIRE_THROWS_WITH(fsm.react(started), Equals("Transition started not allowed from launching state"));
    REQUIRE_THROWS_WITH(fsm.react(stop), Equals("Transition stop not allowed from launching state"));
    REQUIRE_THROWS_WITH(fsm.react(stopped), Equals("Transition stopped not allowed from launching state"));
    REQUIRE_THROWS_WITH(fsm.react(interrupt), Equals("Transition interrupt not allowed from launching state"));
    REQUIRE_THROWS_WITH(fsm.react(interrupted), Equals("Transition interrupted not allowed from launching state"));
    REQUIRE(fsm.isAllowed(failure));
    INFO("launching succeeded");

    satellite.progressFsm();
    REQUIRE(fsm.getState() == State::ORBIT);
    std::this_thread::sleep_for(5ms); // Give some to log in the correct order
    // Allowed in ORBIT: start, land, reconfigure, interrupt, failure
    REQUIRE_THROWS_WITH(fsm.react(initialize), Equals("Transition initialize not allowed from ORBIT state"));
    REQUIRE_THROWS_WITH(fsm.react(initialized), Equals("Transition initialized not allowed from ORBIT state"));
    REQUIRE_THROWS_WITH(fsm.react(launch), Equals("Transition launch not allowed from ORBIT state"));
    REQUIRE_THROWS_WITH(fsm.react(launched), Equals("Transition launched not allowed from ORBIT state"));
    REQUIRE(fsm.isAllowed(land));
    REQUIRE_THROWS_WITH(fsm.react(landed), Equals("Transition landed not allowed from ORBIT state"));
    REQUIRE(fsm.isAllowed(reconfigure));
    REQUIRE_THROWS_WITH(fsm.react(reconfigured), Equals("Transition reconfigured not allowed from ORBIT state"));
    REQUIRE(fsm.isAllowed(start));
    REQUIRE_THROWS_WITH(fsm.react(started), Equals("Transition started not allowed from ORBIT state"));
    REQUIRE_THROWS_WITH(fsm.react(stop), Equals("Transition stop not allowed from ORBIT state"));
    REQUIRE_THROWS_WITH(fsm.react(stopped), Equals("Transition stopped not allowed from ORBIT state"));
    REQUIRE(fsm.isAllowed(interrupt));
    REQUIRE_THROWS_WITH(fsm.react(interrupted), Equals("Transition interrupted not allowed from ORBIT state"));
    REQUIRE(fsm.isAllowed(failure));
    INFO("ORBIT succeeded");

    fsm.react(reconfigure, Configuration());
    REQUIRE(fsm.getState() == State::reconfiguring);
    std::this_thread::sleep_for(5ms); // Give some to log in the correct order
    // Allowed in reconfiguring: reconfigured, interrupt, failure
    REQUIRE_THROWS_WITH(fsm.react(initialize), Equals("Transition initialize not allowed from reconfiguring state"));
    REQUIRE_THROWS_WITH(fsm.react(initialized), Equals("Transition initialized not allowed from reconfiguring state"));
    REQUIRE_THROWS_WITH(fsm.react(launch), Equals("Transition launch not allowed from reconfiguring state"));
    REQUIRE_THROWS_WITH(fsm.react(launched), Equals("Transition launched not allowed from reconfiguring state"));
    REQUIRE_THROWS_WITH(fsm.react(land), Equals("Transition land not allowed from reconfiguring state"));
    REQUIRE_THROWS_WITH(fsm.react(landed), Equals("Transition landed not allowed from reconfiguring state"));
    REQUIRE_THROWS_WITH(fsm.react(reconfigure), Equals("Transition reconfigure not allowed from reconfiguring state"));
    REQUIRE(fsm.isAllowed(reconfigured));
    REQUIRE_THROWS_WITH(fsm.react(start), Equals("Transition start not allowed from reconfiguring state"));
    REQUIRE_THROWS_WITH(fsm.react(started), Equals("Transition started not allowed from reconfiguring state"));
    REQUIRE_THROWS_WITH(fsm.react(stop), Equals("Transition stop not allowed from reconfiguring state"));
    REQUIRE_THROWS_WITH(fsm.react(stopped), Equals("Transition stopped not allowed from reconfiguring state"));
    REQUIRE_THROWS_WITH(fsm.react(interrupt), Equals("Transition interrupt not allowed from reconfiguring state"));
    REQUIRE_THROWS_WITH(fsm.react(interrupted), Equals("Transition interrupted not allowed from reconfiguring state"));
    REQUIRE(fsm.isAllowed(failure));
    INFO("reconfiguring succeeded");

    satellite.progressFsm();
    fsm.react(start, "run_0");
    REQUIRE(fsm.getState() == State::starting);
    std::this_thread::sleep_for(5ms); // Give some to log in the correct order
    // Allowed in starting: started, interrupt, failure
    REQUIRE_THROWS_WITH(fsm.react(initialize), Equals("Transition initialize not allowed from starting state"));
    REQUIRE_THROWS_WITH(fsm.react(initialized), Equals("Transition initialized not allowed from starting state"));
    REQUIRE_THROWS_WITH(fsm.react(launch), Equals("Transition launch not allowed from starting state"));
    REQUIRE_THROWS_WITH(fsm.react(launched), Equals("Transition launched not allowed from starting state"));
    REQUIRE_THROWS_WITH(fsm.react(land), Equals("Transition land not allowed from starting state"));
    REQUIRE_THROWS_WITH(fsm.react(landed), Equals("Transition landed not allowed from starting state"));
    REQUIRE_THROWS_WITH(fsm.react(reconfigure), Equals("Transition reconfigure not allowed from starting state"));
    REQUIRE_THROWS_WITH(fsm.react(reconfigured), Equals("Transition reconfigured not allowed from starting state"));
    REQUIRE_THROWS_WITH(fsm.react(start), Equals("Transition start not allowed from starting state"));
    REQUIRE(fsm.isAllowed(started));
    REQUIRE_THROWS_WITH(fsm.react(stop), Equals("Transition stop not allowed from starting state"));
    REQUIRE_THROWS_WITH(fsm.react(stopped), Equals("Transition stopped not allowed from starting state"));
    REQUIRE_THROWS_WITH(fsm.react(interrupt), Equals("Transition interrupt not allowed from starting state"));
    REQUIRE_THROWS_WITH(fsm.react(interrupted), Equals("Transition interrupted not allowed from starting state"));
    REQUIRE(fsm.isAllowed(failure));
    INFO("starting succeeded");

    satellite.progressFsm();
    REQUIRE(fsm.getState() == State::RUN);
    std::this_thread::sleep_for(5ms); // Give some to log in the correct order
    // Allowed in RUN: stop, interrupt, failure
    REQUIRE_THROWS_WITH(fsm.react(initialize), Equals("Transition initialize not allowed from RUN state"));
    REQUIRE_THROWS_WITH(fsm.react(initialized), Equals("Transition initialized not allowed from RUN state"));
    REQUIRE_THROWS_WITH(fsm.react(launch), Equals("Transition launch not allowed from RUN state"));
    REQUIRE_THROWS_WITH(fsm.react(launched), Equals("Transition launched not allowed from RUN state"));
    REQUIRE_THROWS_WITH(fsm.react(land), Equals("Transition land not allowed from RUN state"));
    REQUIRE_THROWS_WITH(fsm.react(landed), Equals("Transition landed not allowed from RUN state"));
    REQUIRE_THROWS_WITH(fsm.react(reconfigure), Equals("Transition reconfigure not allowed from RUN state"));
    REQUIRE_THROWS_WITH(fsm.react(reconfigured), Equals("Transition reconfigured not allowed from RUN state"));
    REQUIRE_THROWS_WITH(fsm.react(start), Equals("Transition start not allowed from RUN state"));
    REQUIRE_THROWS_WITH(fsm.react(started), Equals("Transition started not allowed from RUN state"));
    REQUIRE(fsm.isAllowed(stop));
    REQUIRE_THROWS_WITH(fsm.react(stopped), Equals("Transition stopped not allowed from RUN state"));
    REQUIRE(fsm.isAllowed(interrupt));
    REQUIRE_THROWS_WITH(fsm.react(interrupted), Equals("Transition interrupted not allowed from RUN state"));
    REQUIRE(fsm.isAllowed(failure));
    INFO("RUN succeeded");

    fsm.react(stop);
    REQUIRE(fsm.getState() == State::stopping);
    std::this_thread::sleep_for(5ms); // Give some to log in the correct order
    // Allowed in stopping: stopped, interrupt, failure
    REQUIRE_THROWS_WITH(fsm.react(initialize), Equals("Transition initialize not allowed from stopping state"));
    REQUIRE_THROWS_WITH(fsm.react(initialized), Equals("Transition initialized not allowed from stopping state"));
    REQUIRE_THROWS_WITH(fsm.react(launch), Equals("Transition launch not allowed from stopping state"));
    REQUIRE_THROWS_WITH(fsm.react(launched), Equals("Transition launched not allowed from stopping state"));
    REQUIRE_THROWS_WITH(fsm.react(land), Equals("Transition land not allowed from stopping state"));
    REQUIRE_THROWS_WITH(fsm.react(landed), Equals("Transition landed not allowed from stopping state"));
    REQUIRE_THROWS_WITH(fsm.react(reconfigure), Equals("Transition reconfigure not allowed from stopping state"));
    REQUIRE_THROWS_WITH(fsm.react(reconfigured), Equals("Transition reconfigured not allowed from stopping state"));
    REQUIRE_THROWS_WITH(fsm.react(start), Equals("Transition start not allowed from stopping state"));
    REQUIRE_THROWS_WITH(fsm.react(started), Equals("Transition started not allowed from stopping state"));
    REQUIRE_THROWS_WITH(fsm.react(stop), Equals("Transition stop not allowed from stopping state"));
    REQUIRE(fsm.isAllowed(stopped));
    REQUIRE_THROWS_WITH(fsm.react(interrupt), Equals("Transition interrupt not allowed from stopping state"));
    REQUIRE_THROWS_WITH(fsm.react(interrupted), Equals("Transition interrupted not allowed from stopping state"));
    REQUIRE(fsm.isAllowed(failure));
    INFO("stopping succeeded");

    satellite.progressFsm();
    fsm.react(land);
    REQUIRE(fsm.getState() == State::landing);
    std::this_thread::sleep_for(5ms); // Give some to log in the correct order
    // Allowed in landing: landed, failure
    REQUIRE_THROWS_WITH(fsm.react(initialize), Equals("Transition initialize not allowed from landing state"));
    REQUIRE_THROWS_WITH(fsm.react(initialized), Equals("Transition initialized not allowed from landing state"));
    REQUIRE_THROWS_WITH(fsm.react(launch), Equals("Transition launch not allowed from landing state"));
    REQUIRE_THROWS_WITH(fsm.react(launched), Equals("Transition launched not allowed from landing state"));
    REQUIRE_THROWS_WITH(fsm.react(land), Equals("Transition land not allowed from landing state"));
    REQUIRE(fsm.isAllowed(landed));
    REQUIRE_THROWS_WITH(fsm.react(reconfigure), Equals("Transition reconfigure not allowed from landing state"));
    REQUIRE_THROWS_WITH(fsm.react(reconfigured), Equals("Transition reconfigured not allowed from landing state"));
    REQUIRE_THROWS_WITH(fsm.react(start), Equals("Transition start not allowed from landing state"));
    REQUIRE_THROWS_WITH(fsm.react(started), Equals("Transition started not allowed from landing state"));
    REQUIRE_THROWS_WITH(fsm.react(stop), Equals("Transition stop not allowed from landing state"));
    REQUIRE_THROWS_WITH(fsm.react(stopped), Equals("Transition stopped not allowed from landing state"));
    REQUIRE_THROWS_WITH(fsm.react(interrupt), Equals("Transition interrupt not allowed from landing state"));
    REQUIRE_THROWS_WITH(fsm.react(interrupted), Equals("Transition interrupted not allowed from landing state"));
    REQUIRE(fsm.isAllowed(failure));
    INFO("landing succeeded");

    satellite.progressFsm();
    fsm.react(launch);
    satellite.progressFsm();
    fsm.react(interrupt);
    REQUIRE(fsm.getState() == State::interrupting);
    std::this_thread::sleep_for(5ms); // Give some to log in the correct order
    // Allowed in interrupting: interrupted, failure
    REQUIRE_THROWS_WITH(fsm.react(initialize), Equals("Transition initialize not allowed from interrupting state"));
    REQUIRE_THROWS_WITH(fsm.react(initialized), Equals("Transition initialized not allowed from interrupting state"));
    REQUIRE_THROWS_WITH(fsm.react(launch), Equals("Transition launch not allowed from interrupting state"));
    REQUIRE_THROWS_WITH(fsm.react(launched), Equals("Transition launched not allowed from interrupting state"));
    REQUIRE_THROWS_WITH(fsm.react(land), Equals("Transition land not allowed from interrupting state"));
    REQUIRE_THROWS_WITH(fsm.react(landed), Equals("Transition landed not allowed from interrupting state"));
    REQUIRE_THROWS_WITH(fsm.react(reconfigure), Equals("Transition reconfigure not allowed from interrupting state"));
    REQUIRE_THROWS_WITH(fsm.react(reconfigured), Equals("Transition reconfigured not allowed from interrupting state"));
    REQUIRE_THROWS_WITH(fsm.react(start), Equals("Transition start not allowed from interrupting state"));
    REQUIRE_THROWS_WITH(fsm.react(started), Equals("Transition started not allowed from interrupting state"));
    REQUIRE_THROWS_WITH(fsm.react(stop), Equals("Transition stop not allowed from interrupting state"));
    REQUIRE_THROWS_WITH(fsm.react(stopped), Equals("Transition stopped not allowed from interrupting state"));
    REQUIRE_THROWS_WITH(fsm.react(interrupt), Equals("Transition interrupt not allowed from interrupting state"));
    REQUIRE(fsm.isAllowed(interrupted));
    REQUIRE(fsm.isAllowed(failure));
    INFO("interrupting succeeded");

    satellite.progressFsm();
    REQUIRE(fsm.getState() == State::SAFE);
    std::this_thread::sleep_for(5ms); // Give some to log in the correct order
    // Allowed in SAFE: initialize, failure
    REQUIRE(fsm.isAllowed(initialize));
    REQUIRE_THROWS_WITH(fsm.react(initialized), Equals("Transition initialized not allowed from SAFE state"));
    REQUIRE_THROWS_WITH(fsm.react(launch), Equals("Transition launch not allowed from SAFE state"));
    REQUIRE_THROWS_WITH(fsm.react(launched), Equals("Transition launched not allowed from SAFE state"));
    REQUIRE_THROWS_WITH(fsm.react(land), Equals("Transition land not allowed from SAFE state"));
    REQUIRE_THROWS_WITH(fsm.react(landed), Equals("Transition landed not allowed from SAFE state"));
    REQUIRE_THROWS_WITH(fsm.react(reconfigure), Equals("Transition reconfigure not allowed from SAFE state"));
    REQUIRE_THROWS_WITH(fsm.react(reconfigured), Equals("Transition reconfigured not allowed from SAFE state"));
    REQUIRE_THROWS_WITH(fsm.react(start), Equals("Transition start not allowed from SAFE state"));
    REQUIRE_THROWS_WITH(fsm.react(started), Equals("Transition started not allowed from SAFE state"));
    REQUIRE_THROWS_WITH(fsm.react(stop), Equals("Transition stop not allowed from SAFE state"));
    REQUIRE_THROWS_WITH(fsm.react(stopped), Equals("Transition stopped not allowed from SAFE state"));
    REQUIRE_THROWS_WITH(fsm.react(interrupt), Equals("Transition interrupt not allowed from SAFE state"));
    REQUIRE_THROWS_WITH(fsm.react(interrupted), Equals("Transition interrupted not allowed from SAFE state"));
    REQUIRE(fsm.isAllowed(failure));
    INFO("SAFE succeeded");

    fsm.react(failure);
    REQUIRE(fsm.getState() == State::ERROR);
    std::this_thread::sleep_for(5ms); // Give some to log in the correct order
    // Allowed in ERROR: initialize
    REQUIRE(fsm.isAllowed(initialize));
    REQUIRE_THROWS_WITH(fsm.react(initialized), Equals("Transition initialized not allowed from ERROR state"));
    REQUIRE_THROWS_WITH(fsm.react(launch), Equals("Transition launch not allowed from ERROR state"));
    REQUIRE_THROWS_WITH(fsm.react(launched), Equals("Transition launched not allowed from ERROR state"));
    REQUIRE_THROWS_WITH(fsm.react(land), Equals("Transition land not allowed from ERROR state"));
    REQUIRE_THROWS_WITH(fsm.react(landed), Equals("Transition landed not allowed from ERROR state"));
    REQUIRE_THROWS_WITH(fsm.react(reconfigure), Equals("Transition reconfigure not allowed from ERROR state"));
    REQUIRE_THROWS_WITH(fsm.react(reconfigured), Equals("Transition reconfigured not allowed from ERROR state"));
    REQUIRE_THROWS_WITH(fsm.react(start), Equals("Transition start not allowed from ERROR state"));
    REQUIRE_THROWS_WITH(fsm.react(started), Equals("Transition started not allowed from ERROR state"));
    REQUIRE_THROWS_WITH(fsm.react(stop), Equals("Transition stop not allowed from ERROR state"));
    REQUIRE_THROWS_WITH(fsm.react(stopped), Equals("Transition stopped not allowed from ERROR state"));
    REQUIRE_THROWS_WITH(fsm.react(interrupt), Equals("Transition interrupt not allowed from ERROR state"));
    REQUIRE_THROWS_WITH(fsm.react(interrupted), Equals("Transition interrupted not allowed from ERROR state"));
    REQUIRE_THROWS_WITH(fsm.react(failure), Equals("Transition failure not allowed from ERROR state"));
    INFO("ERROR succeeded");

    // Reset
    fsm.react(Transition::initialize, Configuration());
    satellite.progressFsm();
    REQUIRE(fsm.getState() == State::INIT);
}

TEST_CASE("FSM callbacks", "[satellite][satellite::fsm]") {
    DummySatellite satellite {};
    auto& fsm = satellite.getFSM();

    std::atomic_bool throw_cb = false;
    std::atomic_int cb_count = 0;
    fsm.registerStateCallback("test", [&](State state) {
        const auto local_count = ++cb_count;
        LOG(DEBUG) << "State callback with state " << to_string(state) << ", count " << local_count;
        if(throw_cb) {
            throw Exception("Throwing in state callback as requested");
        }
    });

    // Initialize, callbacks for initializing and INIT
    satellite.reactFSM(Transition::initialize, Configuration());
    REQUIRE(cb_count.load() == 2);

    // Launch, throw in callback, callbacks for launching and ORBIT
    throw_cb = true;
    satellite.reactFSM(Transition::launch);
    REQUIRE(cb_count.load() == 4);

    fsm.unregisterStateCallback("test");
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
