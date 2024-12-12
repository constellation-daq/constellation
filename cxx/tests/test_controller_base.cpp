/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <thread>
#include <tuple>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <msgpack.hpp>

#include "constellation/controller/Controller.hpp"
#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/FSM.hpp"

#include "chirp_mock.hpp"
#include "dummy_controller.hpp"
#include "dummy_satellite.hpp"

using namespace Catch::Matchers;
using namespace constellation;
using namespace constellation::config;
using namespace constellation::controller;
using namespace constellation::message;
using namespace constellation::protocol;
using namespace constellation::satellite;
using namespace constellation::utils;
using namespace std::chrono_literals;

// Workaround for LLVM issue https://github.com/llvm/llvm-project/issues/113087
#include <catch2/internal/catch_decomposer.hpp>
namespace std {
    template <> struct tuple_size<Catch::Decomposer> {
        static constexpr size_t value = 1;
    };
} // namespace std

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Satellite connecting", "[controller]") {
    // Create CHIRP manager for control service discovery
    auto chirp_manager = create_chirp_manager();

    // Create and start controller
    DummyController controller {"ctrl"};
    controller.start();

    // No connections at present:
    REQUIRE(controller.getConnectionCount() == 0);

    // Create and start satellite
    DummySatellite satellite {};
    chirp_mock_service("Dummy.sat1", chirp::CONTROL, satellite.getCommandPort());

    // Check that satellite connected
    while(controller.getConnectionCount() < 1) {
        std::this_thread::sleep_for(50ms);
    }
    REQUIRE(controller.getConnections().contains("Dummy.sat1"));
    REQUIRE(controller.isInState(CSCP::State::NEW));

    // Stop controller
    controller.stop();

    // Check that all satellites have been removed
    REQUIRE(controller.getConnectionCount() == 0);

    // Exit satellite
    satellite.exit();
}

TEST_CASE("Attempt connection from satellites with same canonical name", "[controller]") {
    // Create CHIRP manager for control service discovery
    auto chirp_manager = create_chirp_manager();

    // Create and start controller
    DummyController controller {"ctrl"};
    controller.start();

    // No connections at present:
    REQUIRE(controller.getConnectionCount() == 0);

    // Create and start satellite
    DummySatellite satellite1 {"a"};
    chirp_mock_service("Dummy.a", chirp::CONTROL, satellite1.getCommandPort());

    // Check that satellite connected
    while(controller.getConnectionCount() < 1) {
        std::this_thread::sleep_for(50ms);
    }
    REQUIRE(controller.getConnections().contains("Dummy.a"));
    REQUIRE(controller.isInState(CSCP::State::NEW));

    // Create and start second satellite with same canonical name
    DummySatellite satellite2 {"a"};
    chirp_mock_service("Dummy.a", chirp::CONTROL, satellite2.getCommandPort());

    // Check that second satellite was not connected
    REQUIRE(controller.getConnectionCount() == 1);

    // Stop controller and exit satellites
    controller.stop();
    satellite1.exit();
    satellite2.exit();
}

TEST_CASE("Satellite departing", "[controller]") {
    // Create CHIRP manager for control service discovery
    auto chirp_manager = create_chirp_manager();

    // Create and start controller
    DummyController controller {"ctrl"};
    controller.start();

    // No connections at present:
    REQUIRE(controller.getConnectionCount() == 0);

    // Create and start satellite
    DummySatellite satellite {};
    chirp_mock_service("Dummy.sat1", chirp::CONTROL, satellite.getCommandPort());

    // Check that satellite connected
    while(controller.getConnectionCount() < 1) {
        std::this_thread::sleep_for(50ms);
    }
    REQUIRE(controller.getConnections().contains("Dummy.sat1"));
    REQUIRE(controller.isInState(CSCP::State::NEW));

    // Depart the satellite
    chirp_mock_service("Dummy.sat1", chirp::CONTROL, satellite.getCommandPort(), false);

    // Wait for CHIRP message to be processed:
    while(controller.getConnectionCount() > 0) {
        std::this_thread::sleep_for(50ms);
    }

    // Stop controller and exit satellite
    controller.stop();
    satellite.exit();
}

TEST_CASE("State Updates are propagated", "[controller]") {
    // Create CHIRP manager for control service discovery
    auto chirp_manager = create_chirp_manager();

    // Create and start controller
    DummyController controller {"ctrl"};
    controller.start();

    // Create and start satellite
    DummySatellite satellite {"a"};
    chirp_mock_service("Dummy.a", chirp::CONTROL, satellite.getCommandPort());

    // Wait for connection
    while(controller.getConnectionCount() < 1) {
        std::this_thread::sleep_for(50ms);
    }

    // Wait for connection update to have propagated
    controller.waitPropagateUpdate();
    REQUIRE(controller.lastPropagateUpdate() ==
            std::make_tuple(DummyController::UpdateType::ADDED, std::size_t(0), std::size_t(1)));

    // Check that state updates were propagated:
    controller.waitReachedState(CSCP::State::NEW, true);

    // Create and start second satellite
    DummySatellite satellite2 {"z"};
    chirp_mock_service("Dummy.z", chirp::CONTROL, satellite2.getCommandPort());

    // Wait for connection
    while(controller.getConnectionCount() < 2) {
        std::this_thread::sleep_for(50ms);
    }

    // Wait for connection update to have propagated
    controller.waitPropagateUpdate();
    REQUIRE(controller.lastPropagateUpdate() ==
            std::make_tuple(DummyController::UpdateType::ADDED, std::size_t(1), std::size_t(2)));

    // Check that state updates were propagated:
    controller.waitReachedState(CSCP::State::NEW, true);

    // Stop controller and exit satellites
    controller.stop();
    satellite.exit();
    satellite2.exit();
}

TEST_CASE("Satellite state updates are received", "[controller]") {
    // Create CHIRP manager for control service discovery
    auto chirp_manager = create_chirp_manager();

    // Create and start controller
    DummyController controller {"ctrl"};
    controller.start();

    // Create and start satellite
    DummySatellite satellite {"a"};
    chirp_mock_service("Dummy.a", chirp::CONTROL, satellite.getCommandPort());
    chirp_mock_service("Dummy.a", chirp::HEARTBEAT, satellite.getHeartbeatPort());

    // Wait for connection
    while(controller.getConnectionCount() < 1) {
        std::this_thread::sleep_for(50ms);
    }

    // Check that state updates were propagated:
    controller.waitReachedState(CSCP::State::NEW, true);

    // Initialize satellite
    satellite.reactFSM(FSM::Transition::initialize, Configuration());

    // Check that state updates were received:
    controller.waitReachedState(CSCP::State::INIT, true);

    // Stop controller and exit satellite
    controller.stop();
    satellite.exit();
}

TEST_CASE("Mixed and global states are reported", "[controller]") {
    // Create CHIRP manager for control service discovery
    auto chirp_manager = create_chirp_manager();

    // Create and start controller
    DummyController controller {"ctrl"};
    controller.start();

    // Create and start satellite
    DummySatellite satelliteA {"a"};
    DummySatellite satelliteB {"b"};
    chirp_mock_service("Dummy.a", chirp::CONTROL, satelliteA.getCommandPort());
    chirp_mock_service("Dummy.a", chirp::HEARTBEAT, satelliteA.getHeartbeatPort());
    chirp_mock_service("Dummy.b", chirp::CONTROL, satelliteB.getCommandPort());
    chirp_mock_service("Dummy.b", chirp::HEARTBEAT, satelliteB.getHeartbeatPort());

    while(controller.getConnectionCount() < 2) {
        std::this_thread::sleep_for(50ms);
    }

    // Check that state updates were propagated:
    controller.waitReachedState(CSCP::State::NEW, true);
    REQUIRE(controller.getLowestState() == CSCP::State::NEW);
    REQUIRE(controller.isInGlobalState());

    // Initialize satelliteA
    satelliteA.reactFSM(FSM::Transition::initialize, Configuration());

    // Check that state is mentioned as mixed:
    controller.waitReachedState(CSCP::State::NEW, false);
    REQUIRE(controller.getLowestState() == CSCP::State::NEW);
    REQUIRE_FALSE(controller.isInGlobalState());

    // Initialize satelliteB
    satelliteB.reactFSM(FSM::Transition::initialize, Configuration());

    // Check that state is INIT and mentioned as global:
    controller.waitReachedState(CSCP::State::INIT, true);
    REQUIRE(controller.getLowestState() == CSCP::State::INIT);
    REQUIRE(controller.isInGlobalState());

    // Stop controller and exit satellites
    controller.stop();
    satelliteA.exit();
    satelliteB.exit();
}

TEST_CASE("Controller commands are sent and answered", "[controller]") {
    // Create CHIRP manager for control service discovery
    auto chirp_manager = create_chirp_manager();

    // Create and start controller
    DummyController controller {"ctrl"};
    controller.start();

    // Create and start satellite
    DummySatellite satelliteA {"a"};
    DummySatellite satelliteB {"b"};
    chirp_mock_service("Dummy.a", chirp::CONTROL, satelliteA.getCommandPort());
    chirp_mock_service("Dummy.a", chirp::HEARTBEAT, satelliteA.getHeartbeatPort());
    chirp_mock_service("Dummy.b", chirp::CONTROL, satelliteB.getCommandPort());
    chirp_mock_service("Dummy.b", chirp::HEARTBEAT, satelliteB.getHeartbeatPort());

    // Await connections
    while(controller.getConnectionCount() < 2) {
        std::this_thread::sleep_for(50ms);
    }

    // Send command to single satellite with payload
    const auto msg = controller.sendCommand("Dummy.a", "initialize", Dictionary());
    REQUIRE(msg.getVerb().first == CSCP1Message::Type::SUCCESS);
    satelliteA.progressFsm();

    // Check that state is mixed:
    controller.waitReachedState(CSCP::State::NEW, false);
    REQUIRE(controller.getLowestState() == CSCP::State::NEW);
    REQUIRE_FALSE(controller.isInGlobalState());

    // Send command to single satellite with prepared CSCP1Message:
    auto msg_send = CSCP1Message({"ctrl"}, {CSCP1Message::Type::REQUEST, "launch"});
    const auto msg_rply = controller.sendCommand("Dummy.a", msg_send);
    REQUIRE(msg_rply.getVerb().first == CSCP1Message::Type::SUCCESS);
    satelliteA.progressFsm();

    // Check that state is mixed:
    controller.waitReachedState(CSCP::State::NEW, false);
    REQUIRE(controller.getLowestState() == CSCP::State::NEW);
    REQUIRE_FALSE(controller.isInGlobalState());

    // Send command to all satellites with prepared CSCP1Message
    const auto msgs_rply = controller.sendCommands(msg_send);
    REQUIRE(msgs_rply.contains("Dummy.a"));
    REQUIRE(msgs_rply.contains("Dummy.b"));
    REQUIRE(msgs_rply.at("Dummy.a").getVerb().first == CSCP1Message::Type::INVALID);
    REQUIRE(msgs_rply.at("Dummy.b").getVerb().first == CSCP1Message::Type::INVALID);

    // Check that state is mixed:
    controller.waitReachedState(CSCP::State::NEW, false);
    REQUIRE(controller.getLowestState() == CSCP::State::NEW);
    REQUIRE_FALSE(controller.isInGlobalState());

    // Land satelliteA again
    const auto msg_lnd = controller.sendCommand("Dummy.a", "land");
    REQUIRE(msg_lnd.getVerb().first == CSCP1Message::Type::SUCCESS);
    satelliteA.progressFsm();

    // Check that state is mixed:
    controller.waitReachedState(CSCP::State::NEW, false);
    REQUIRE(controller.getLowestState() == CSCP::State::NEW);
    REQUIRE_FALSE(controller.isInGlobalState());

    // Send command to all satellites with same payload
    const auto msgs = controller.sendCommands("initialize", Dictionary());
    REQUIRE(msgs.contains("Dummy.a"));
    REQUIRE(msgs.contains("Dummy.b"));
    REQUIRE(msgs.at("Dummy.a").getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE(msgs.at("Dummy.b").getVerb().first == CSCP1Message::Type::SUCCESS);
    satelliteA.progressFsm();
    satelliteB.progressFsm();

    // Check that state is global:
    controller.waitReachedState(CSCP::State::INIT, true);
    REQUIRE(controller.getLowestState() == CSCP::State::INIT);
    REQUIRE(controller.isInGlobalState());

    // Stop controller and exit satellites
    controller.stop();
    satelliteA.exit();
    satelliteB.exit();
}

TEST_CASE("Controller sends command with different payloads", "[controller]") {
    // Create CHIRP manager for control service discovery
    auto chirp_manager = create_chirp_manager();

    // Create and start controller
    DummyController controller {"ctrl"};
    controller.start();

    // Create and start satellite
    DummySatellite satelliteA {"a"};
    DummySatellite satelliteB {"b"};
    chirp_mock_service("Dummy.a", chirp::CONTROL, satelliteA.getCommandPort());
    chirp_mock_service("Dummy.a", chirp::HEARTBEAT, satelliteA.getHeartbeatPort());
    chirp_mock_service("Dummy.b", chirp::CONTROL, satelliteB.getCommandPort());
    chirp_mock_service("Dummy.b", chirp::HEARTBEAT, satelliteB.getHeartbeatPort());

    // Await connection
    while(controller.getConnectionCount() < 2) {
        std::this_thread::sleep_for(50ms);
    }

    // Send command to single satellite with payload
    Dictionary config_a;
    config_a["_heartbeat_interval"] = 3;
    Dictionary config_b;
    config_b["_heartbeat_interval"] = 5;
    const auto payload_ = std::map<std::string, Controller::CommandPayload> {{"Dummy.a", config_a}, {"Dummy.b", config_b}};
    const auto msgs = controller.sendCommands("initialize", payload_);
    REQUIRE(msgs.contains("Dummy.a"));
    REQUIRE(msgs.contains("Dummy.b"));
    REQUIRE(msgs.at("Dummy.a").getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE(msgs.at("Dummy.b").getVerb().first == CSCP1Message::Type::SUCCESS);
    satelliteA.progressFsm();
    satelliteB.progressFsm();

    // Check that state is global:
    controller.waitReachedState(CSCP::State::INIT, true);
    REQUIRE(controller.getLowestState() == CSCP::State::INIT);
    REQUIRE(controller.isInGlobalState());

    // Check that satellites received correct configuration:
    const auto rply = controller.sendCommands("get_config");
    REQUIRE(rply.contains("Dummy.a"));
    REQUIRE(rply.contains("Dummy.b"));
    REQUIRE(rply.at("Dummy.a").getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE(rply.at("Dummy.b").getVerb().first == CSCP1Message::Type::SUCCESS);
    const auto satA_cfg = Dictionary::disassemble(rply.at("Dummy.a").getPayload());
    const auto satB_cfg = Dictionary::disassemble(rply.at("Dummy.b").getPayload());
    REQUIRE(satA_cfg.contains("_heartbeat_interval"));
    REQUIRE(satB_cfg.contains("_heartbeat_interval"));
    REQUIRE(satA_cfg.at("_heartbeat_interval").get<std::int64_t>() == 3);
    REQUIRE(satB_cfg.at("_heartbeat_interval").get<std::int64_t>() == 5);

    // Stop controller and exit satellites
    controller.stop();
    satelliteA.exit();
    satelliteB.exit();
}

TEST_CASE("Erroneous attempts to send commands", "[controller]") {

    // Create CHIRP manager for control service discovery
    auto chirp_manager = create_chirp_manager();

    // Create and start controller
    DummyController controller {"ctrl"};
    controller.start();

    // Create and start satellite
    DummySatellite satelliteA {"a"};
    chirp_mock_service("Dummy.a", chirp::CONTROL, satelliteA.getCommandPort());
    chirp_mock_service("Dummy.a", chirp::HEARTBEAT, satelliteA.getHeartbeatPort());

    // Await connection
    while(controller.getConnectionCount() < 1) {
        std::this_thread::sleep_for(50ms);
    }

    // Send command to unknown target satellite:
    const auto msg_rply_unknown = controller.sendCommand("Dummy.b", "launch");
    REQUIRE(msg_rply_unknown.getVerb().first == CSCP1Message::Type::ERROR);
    REQUIRE_THAT(to_string(msg_rply_unknown.getVerb().second), Equals("Target satellite is unknown to controller"));

    // Send command with illegal verb to single satellite:
    auto msg_err = CSCP1Message({"ctrl"}, {CSCP1Message::Type::UNKNOWN, "launch"});
    const auto msg_rply_err = controller.sendCommand("Dummy.a", msg_err);
    REQUIRE(msg_rply_err.getVerb().first == CSCP1Message::Type::ERROR);
    REQUIRE_THAT(to_string(msg_rply_err.getVerb().second), Equals("Can only send command messages of type REQUEST"));

    // Stop controller and exit satellites
    controller.stop();
    satelliteA.exit();
}

TEST_CASE("Controller can read run identifier and time", "[controller]") {
    // Create CHIRP manager for control service discovery
    auto chirp_manager = create_chirp_manager();

    // Create and start controller
    DummyController controller {"ctrl"};
    controller.start();

    // Create and start satellite
    DummySatellite satellite {"a"};
    chirp_mock_service("Dummy.a", chirp::CONTROL, satellite.getCommandPort());
    chirp_mock_service("Dummy.a", chirp::HEARTBEAT, satellite.getHeartbeatPort());

    // Await connection
    while(controller.getConnectionCount() < 1) {
        std::this_thread::sleep_for(50ms);
    }

    // Check that state updates were propagated:
    controller.waitReachedState(CSCP::State::NEW, true);

    // Read the run identifier and start time from the running constellation:
    REQUIRE(controller.getRunIdentifier().empty());
    const auto no_start_time = controller.getRunStartTime();
    REQUIRE_FALSE(no_start_time.has_value());

    // Initialize, launch and start satellite, and check that state updates were propagated
    satellite.reactFSM(FSM::Transition::initialize, Configuration());
    controller.waitReachedState(CSCP::State::INIT, true);
    satellite.reactFSM(FSM::Transition::launch);
    controller.waitReachedState(CSCP::State::ORBIT, true);
    satellite.reactFSM(FSM::Transition::start, "this_run_0001");
    controller.waitReachedState(CSCP::State::RUN, true);

    // Read the run identifier and start time from the running constellation:
    REQUIRE_THAT(controller.getRunIdentifier(), Equals("this_run_0001"));
    const auto start_time = controller.getRunStartTime();
    REQUIRE(start_time.has_value());
    REQUIRE(start_time.value() < std::chrono::system_clock::now()); // NOLINT(bugprone-unchecked-optional-access)

    // Stop controller and exit satellites
    controller.stop();
    satellite.exit();
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
