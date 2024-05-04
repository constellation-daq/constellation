/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <utility>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <msgpack.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/ports.hpp"
#include "constellation/satellite/Satellite.hpp"
#include "constellation/satellite/SatelliteImplementation.hpp"

using namespace Catch::Matchers;
using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::satellite;
using namespace constellation::utils;
using namespace std::literals::chrono_literals;

class DummySatellite : public Satellite {
public:
    DummySatellite() : Satellite("Dummy", "sat1") {
        support_reconfigure();
        set_status("just started!");
    }
};

class CSCPSender {
public:
    CSCPSender(Port port) : req_(context_, zmq::socket_type::req) {
        req_.connect("tcp://127.0.0.1:" + std::to_string(port));
    }
    void send(std::span<const std::byte> message, zmq::send_flags send_flags = zmq::send_flags::none) {
        zmq::message_t zmq_msg {message.data(), message.size()};
        req_.send(zmq_msg, send_flags);
    }
    void send(CSCP1Message& message) { message.assemble().send(req_); }
    void send_command(std::string command) {
        auto msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, std::move(command)});
        send(msg);
    }
    CSCP1Message recv() {
        zmq::multipart_t zmq_msgs {};
        zmq_msgs.recv(req_);
        return CSCP1Message::disassemble(zmq_msgs);
    }

private:
    zmq::context_t context_;
    zmq::socket_t req_;
};

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Get commands", "[satellite]") {
    // Create and start satellite
    auto satellite = std::make_shared<DummySatellite>();
    auto satellite_implementation = SatelliteImplementation(satellite);
    satellite_implementation.start();

    // Create sender
    CSCPSender sender {satellite_implementation.getPort()};

    // get_name
    sender.send_command("get_name");
    auto recv_msg_get_name = sender.recv();
    REQUIRE(recv_msg_get_name.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_name.getVerb().second), Equals(satellite->getCanonicalName()));
    REQUIRE(!recv_msg_get_name.hasPayload());

    // get_commands
    sender.send_command("get_commands");
    auto recv_msg_get_commands = sender.recv();
    REQUIRE(recv_msg_get_commands.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_commands.getVerb().second), Equals("Commands attached in payload"));
    REQUIRE(recv_msg_get_commands.hasPayload());
    const auto get_commands_dict = Dictionary::disassemble(*recv_msg_get_commands.getPayload());
    REQUIRE(get_commands_dict.contains("get_commands"));
    REQUIRE(get_commands_dict.at("stop").get<std::string>() == "Stop satellite");

    // get_state
    sender.send_command("get_state");
    auto recv_msg_get_state = sender.recv();
    REQUIRE(recv_msg_get_state.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_state.getVerb().second), Equals("NEW"));
    REQUIRE(!recv_msg_get_state.hasPayload());

    // get_status
    sender.send_command("get_status");
    auto recv_msg_get_status = sender.recv();
    REQUIRE(recv_msg_get_status.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_status.getVerb().second), Equals("just started!"));
    REQUIRE(!recv_msg_get_status.hasPayload());

    // get_config
    sender.send_command("get_config");
    auto recv_msg_get_config = sender.recv();
    REQUIRE(recv_msg_get_config.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_config.getVerb().second), Equals("Configuration attached in payload"));
    REQUIRE(recv_msg_get_config.hasPayload());
    const auto config = Configuration(Dictionary::disassemble(*recv_msg_get_config.getPayload()));
    REQUIRE(config.size() == 0);
    // TODO(stephan.lachnit): test with a non-empty configuration
}

TEST_CASE("Case insensitive", "[satellite]") {
    // Create and start satellite
    auto satellite = std::make_shared<DummySatellite>();
    auto satellite_implementation = SatelliteImplementation(satellite);
    satellite_implementation.start();

    // Create sender
    CSCPSender sender {satellite_implementation.getPort()};

    // get_name with non-lower-case case
    sender.send_command("GeT_nAmE");
    auto recv_msg_get_name = sender.recv();
    REQUIRE(recv_msg_get_name.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_name.getVerb().second), Equals(satellite->getCanonicalName()));
    REQUIRE(!recv_msg_get_name.hasPayload());
}

TEST_CASE("Transitions", "[satellite]") {
    // Create and start satellite
    auto satellite = std::make_shared<DummySatellite>();
    auto satellite_implementation = SatelliteImplementation(satellite);
    satellite_implementation.start();

    // Create sender
    CSCPSender sender {satellite_implementation.getPort()};

    // Send initialize
    auto initialize_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "initialize"});
    initialize_msg.addPayload(Dictionary().assemble());
    sender.send(initialize_msg);

    // Check reply
    auto recv_msg_initialize = sender.recv();
    REQUIRE(recv_msg_initialize.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_initialize.getVerb().second), Equals("Transition initialize is being initiated"));

    // Check state
    std::this_thread::sleep_for(100ms);
    sender.send_command("get_state");
    auto recv_msg_get_status = sender.recv();
    REQUIRE(recv_msg_get_status.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_status.getVerb().second), Equals("INIT"));
}

TEST_CASE("Catch unknown command", "[satellite]") {
    // Create and start satellite
    auto satellite = std::make_shared<DummySatellite>();
    auto satellite_implementation = SatelliteImplementation(satellite);
    satellite_implementation.start();

    // Create sender
    CSCPSender sender {satellite_implementation.getPort()};

    // Send with unexpected message type
    auto wrong_type_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "get_names"});
    sender.send(wrong_type_msg);
    auto recv_msg_wrong_type = sender.recv();
    REQUIRE(recv_msg_wrong_type.getVerb().first == CSCP1Message::Type::UNKNOWN);
    REQUIRE_THAT(to_string(recv_msg_wrong_type.getVerb().second), Equals("Command \"get_names\" is not known"));
}

TEST_CASE("Catch unexpected message type", "[satellite]") {
    // Create and start satellite
    auto satellite = std::make_shared<DummySatellite>();
    auto satellite_implementation = SatelliteImplementation(satellite);
    satellite_implementation.start();

    // Create sender
    CSCPSender sender {satellite_implementation.getPort()};

    // Send with unexpected message type
    auto wrong_type_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::SUCCESS, "get_name"});
    sender.send(wrong_type_msg);
    auto recv_msg_wrong_type = sender.recv();
    REQUIRE(recv_msg_wrong_type.getVerb().first == CSCP1Message::Type::ERROR);
    REQUIRE_THAT(to_string(recv_msg_wrong_type.getVerb().second), Equals("Can only handle CSCP messages with REQUEST type"));
}

TEST_CASE("Catch invalid protocol", "[satellite]") {
    // Create and start satellite
    auto satellite = std::make_shared<DummySatellite>();
    auto satellite_implementation = SatelliteImplementation(satellite);
    satellite_implementation.start();

    // Create sender
    CSCPSender sender {satellite_implementation.getPort()};

    // Send with invalid protocol
    msgpack::sbuffer sbuf {};
    msgpack::pack(sbuf, "INVALID");
    msgpack::pack(sbuf, "cscp_sender");
    msgpack::pack(sbuf, std::chrono::system_clock::now());
    msgpack::pack(sbuf, Dictionary());
    sender.send({to_byte_ptr(sbuf.data()), sbuf.size()}, zmq::send_flags::sndmore);
    sbuf.clear();
    msgpack::pack(sbuf, std::to_underlying(CSCP1Message::Type::REQUEST));
    msgpack::pack(sbuf, "get_name");
    sender.send({to_byte_ptr(sbuf.data()), sbuf.size()});

    auto recv_msg_invalid_proto = sender.recv();
    REQUIRE(recv_msg_invalid_proto.getVerb().first == CSCP1Message::Type::ERROR);
    REQUIRE_THAT(to_string(recv_msg_invalid_proto.getVerb().second), Equals("Invalid protocol identifier \"INVALID\""));
}

TEST_CASE("Catch unexpected protocol", "[satellite]") {
    // Create and start satellite
    auto satellite = std::make_shared<DummySatellite>();
    auto satellite_implementation = SatelliteImplementation(satellite);
    satellite_implementation.start();

    // Create sender
    CSCPSender sender {satellite_implementation.getPort()};

    // Send with unexpected protocol
    msgpack::sbuffer sbuf {};
    msgpack::pack(sbuf, "CMDP\x01");
    msgpack::pack(sbuf, "cscp_sender");
    msgpack::pack(sbuf, std::chrono::system_clock::now());
    msgpack::pack(sbuf, Dictionary());
    sender.send({to_byte_ptr(sbuf.data()), sbuf.size()}, zmq::send_flags::sndmore);
    sbuf.clear();
    msgpack::pack(sbuf, std::to_underlying(CSCP1Message::Type::REQUEST));
    msgpack::pack(sbuf, "get_name");
    sender.send({to_byte_ptr(sbuf.data()), sbuf.size()});

    auto recv_msg_wrong_proto = sender.recv();
    REQUIRE(recv_msg_wrong_proto.getVerb().first == CSCP1Message::Type::ERROR);
    REQUIRE_THAT(to_string(recv_msg_wrong_proto.getVerb().second),
                 Equals("Received protocol \"CMDP1\" does not match expected identifier \"CSCP1\""));
}

TEST_CASE("Catch incorrect payload", "[satellite]") {
    // Create and start satellite
    auto satellite = std::make_shared<DummySatellite>();
    auto satellite_implementation = SatelliteImplementation(satellite);
    satellite_implementation.start();

    // Create sender
    CSCPSender sender {satellite_implementation.getPort()};

    // Send initialize
    auto initialize_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "initialize"});
    auto initialize_payload = std::make_shared<zmq::message_t>("dummy payload");
    initialize_msg.addPayload(std::move(initialize_payload));
    sender.send(initialize_msg);

    // Check reply
    auto recv_msg_initialize = sender.recv();
    REQUIRE(recv_msg_initialize.getVerb().first == CSCP1Message::Type::INCOMPLETE);
    REQUIRE_THAT(to_string(recv_msg_initialize.getVerb().second),
                 Equals("Transition initialize received incorrect payload"));

    // Check state
    std::this_thread::sleep_for(100ms);
    sender.send_command("get_state");
    auto recv_msg_get_status = sender.recv();
    REQUIRE(recv_msg_get_status.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_status.getVerb().second), Equals("NEW"));
}

TEST_CASE("Catch wrong number of frames", "[satellite]") {
    // Create and start satellite
    auto satellite = std::make_shared<DummySatellite>();
    auto satellite_implementation = SatelliteImplementation(satellite);
    satellite_implementation.start();

    // Create sender
    CSCPSender sender {satellite_implementation.getPort()};

    // Send with wrong number of frames
    msgpack::sbuffer sbuf {};
    msgpack::pack(sbuf, "CSCP\x01");
    msgpack::pack(sbuf, "cscp_sender");
    msgpack::pack(sbuf, std::chrono::system_clock::now());
    msgpack::pack(sbuf, Dictionary());
    sender.send({to_byte_ptr(sbuf.data()), sbuf.size()});

    auto recv_msg_wrong_frames = sender.recv();
    REQUIRE(recv_msg_wrong_frames.getVerb().first == CSCP1Message::Type::ERROR);
    REQUIRE_THAT(to_string(recv_msg_wrong_frames.getVerb().second),
                 Equals("Error decoding message: Incorrect number of message frames"));
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
