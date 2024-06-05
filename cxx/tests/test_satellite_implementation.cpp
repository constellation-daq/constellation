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
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <msgpack.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/ports.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/Satellite.hpp"
#include "constellation/satellite/SatelliteImplementation.hpp"

using namespace Catch::Matchers;
using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::satellite;
using namespace constellation::utils;
using namespace std::literals::chrono_literals;
using namespace std::literals::string_literals;

class DummySatellite : public Satellite {
    // NOLINTBEGIN(readability-convert-member-functions-to-static,readability-make-member-function-const)
    int usr_cmd() { return 2; }
    int usr_cmd_arg(int a) { return 2 * a; }
    std::array<int, 1> usr_cmd_invalid_return() { return {2}; }
    void usr_cmd_void() { value_ = 3; };
    int value_ {2};
    // NOLINTEND(readability-convert-member-functions-to-static,readability-make-member-function-const)

public:
    DummySatellite() : Satellite("Dummy", "sat1") {
        support_reconfigure();
        set_status("just started!");
        register_command("my_cmd", "A User Command", {}, &DummySatellite::usr_cmd, this);
        register_command("my_cmd_arg", "Another User Command", {}, &DummySatellite::usr_cmd_arg, this);
        register_command("my_cmd_invalid_return", "Invalid User Command", {}, &DummySatellite::usr_cmd_invalid_return, this);
        register_command("my_cmd_void", "Command without arguments & return", {}, &DummySatellite::usr_cmd_void, this);
        register_command("my_cmd_state", "Command for RUN state only", {State::RUN}, &DummySatellite::usr_cmd_void, this);
    }
};

class CSCPSender {
public:
    CSCPSender(Port port) : req_(context_, zmq::socket_type::req) { req_.connect("tcp://127.0.0.1:" + to_string(port)); }
    void send(std::span<const std::byte> message, zmq::send_flags send_flags = zmq::send_flags::none) {
        zmq::message_t zmq_msg {message.data(), message.size()};
        req_.send(zmq_msg, send_flags);
    }
    void send(CSCP1Message& message) { message.assemble().send(req_); }
    void sendCommand(std::string command) {
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
    sender.sendCommand("get_name");
    auto recv_msg_get_name = sender.recv();
    REQUIRE(recv_msg_get_name.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_name.getVerb().second), Equals(satellite->getCanonicalName()));
    REQUIRE(!recv_msg_get_name.hasPayload());

    // get_commands
    sender.sendCommand("get_commands");
    auto recv_msg_get_commands = sender.recv();
    REQUIRE(recv_msg_get_commands.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_commands.getVerb().second), Equals("Commands attached in payload"));
    REQUIRE(recv_msg_get_commands.hasPayload());
    const auto get_commands_dict = Dictionary::disassemble(recv_msg_get_commands.getPayload());
    REQUIRE(get_commands_dict.contains("get_commands"));
    REQUIRE(get_commands_dict.at("stop").get<std::string>() == "Stop run");
    REQUIRE(get_commands_dict.contains("my_cmd"));
    REQUIRE_THAT(std::get<std::string>(get_commands_dict.at("my_cmd")),
                 Equals("A User Command\nThis command requires 0 arguments.\nThis command can be called in all states."));
    REQUIRE(get_commands_dict.contains("my_cmd_state"));
    REQUIRE_THAT(
        std::get<std::string>(get_commands_dict.at("my_cmd_state")),
        Equals("Command for RUN state only\nThis command requires 0 arguments.\nThis command can only be called in the "
               "following states: RUN"));

    // get_state
    sender.sendCommand("get_state");
    auto recv_msg_get_state = sender.recv();
    REQUIRE(recv_msg_get_state.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_state.getVerb().second), Equals("NEW"));
    REQUIRE(!recv_msg_get_state.hasPayload());

    // get_status
    sender.sendCommand("get_status");
    auto recv_msg_get_status = sender.recv();
    REQUIRE(recv_msg_get_status.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_status.getVerb().second), Equals("just started!"));
    REQUIRE(!recv_msg_get_status.hasPayload());

    // get_config
    sender.sendCommand("get_config");
    auto recv_msg_get_config = sender.recv();
    REQUIRE(recv_msg_get_config.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_config.getVerb().second), Equals("Configuration attached in payload"));
    REQUIRE(recv_msg_get_config.hasPayload());
    const auto config = Configuration(Dictionary::disassemble(recv_msg_get_config.getPayload()));
    REQUIRE(config.size() == 0);
    // TODO(stephan.lachnit): test with a non-empty configuration
}

TEST_CASE("Satellite name", "[satellite]") {
    class InvalidSatellite : public Satellite {
    public:
        InvalidSatellite() : Satellite("Invalid", "invalid_satellite&name") {}
    };
    REQUIRE_THROWS_MATCHES(std::make_shared<InvalidSatellite>(), RuntimeError, Message("Satellite name is invalid"));
}

TEST_CASE("User commands", "[satellite]") {
    // Create and start satellite
    auto satellite = std::make_shared<DummySatellite>();
    auto satellite_implementation = SatelliteImplementation(satellite);
    satellite_implementation.start();

    // Create sender
    CSCPSender sender {satellite_implementation.getPort()};

    // my_cmd user command
    sender.sendCommand("my_cmd");
    auto recv_msg_usr_cmd = sender.recv();
    REQUIRE(recv_msg_usr_cmd.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_usr_cmd.getVerb().second), Equals(""));
    REQUIRE(recv_msg_usr_cmd.hasPayload());
    const auto& usrmsgpayload = recv_msg_usr_cmd.getPayload();
    const auto usrpayload = msgpack::unpack(to_char_ptr(usrmsgpayload.span().data()), usrmsgpayload.span().size());
    REQUIRE(usrpayload->as<int>() == 2);

    // my_usr_cmd_arg with argument as payload
    auto usr_cmd_arg_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "my_cmd_arg"});
    msgpack::sbuffer sbuf {};
    List args {};
    args.push_back(4);
    msgpack::pack(sbuf, args);
    usr_cmd_arg_msg.addPayload(std::move(sbuf));
    sender.send(usr_cmd_arg_msg);

    auto recv_msg_usr_cmd_arg = sender.recv();
    REQUIRE(recv_msg_usr_cmd_arg.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_usr_cmd_arg.getVerb().second), Equals(""));
    REQUIRE(recv_msg_usr_cmd_arg.hasPayload());
    const auto& usrargmsgpayload = recv_msg_usr_cmd_arg.getPayload();
    const auto usrargpayload = msgpack::unpack(to_char_ptr(usrargmsgpayload.span().data()), usrargmsgpayload.span().size());
    REQUIRE(usrargpayload->as<int>() == 8);

    // my_cmd_void user command without arguments and return value
    sender.sendCommand("my_cmd_void");
    auto recv_msg_usr_cmd_void = sender.recv();
    REQUIRE(recv_msg_usr_cmd_void.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_usr_cmd_void.getVerb().second), Equals(""));
    REQUIRE(!recv_msg_usr_cmd_void.hasPayload());
}

TEST_CASE("Case insensitive", "[satellite]") {
    // Create and start satellite
    auto satellite = std::make_shared<DummySatellite>();
    auto satellite_implementation = SatelliteImplementation(satellite);
    satellite_implementation.start();

    // Create sender
    CSCPSender sender {satellite_implementation.getPort()};

    // get_name with non-lower-case case
    sender.sendCommand("GeT_nAmE");
    auto recv_msg_get_name = sender.recv();
    REQUIRE(recv_msg_get_name.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_name.getVerb().second), Equals(satellite->getCanonicalName()));
    REQUIRE(!recv_msg_get_name.hasPayload());

    // my_cmd user command
    sender.sendCommand("mY_cMd");
    auto recv_msg_usr_cmdn = sender.recv();
    REQUIRE(recv_msg_usr_cmdn.getVerb().first == CSCP1Message::Type::SUCCESS);
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
    std::this_thread::sleep_for(150ms);
    sender.sendCommand("get_state");
    auto recv_msg_get_status = sender.recv();
    REQUIRE(recv_msg_get_status.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_status.getVerb().second), Equals("INIT"));
}

TEST_CASE("Shutdown", "[satellite]") {
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
    auto recv_msg_initialize = sender.recv();
    REQUIRE(recv_msg_initialize.getVerb().first == CSCP1Message::Type::SUCCESS);
    std::this_thread::sleep_for(150ms);

    // Send launch
    auto launch_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "launch"});
    sender.send(launch_msg);
    auto recv_msg_launch = sender.recv();
    REQUIRE(recv_msg_launch.getVerb().first == CSCP1Message::Type::SUCCESS);
    std::this_thread::sleep_for(150ms);

    // Try shutdown & fail
    auto shutdown1_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "shutdown"});
    sender.send(shutdown1_msg);
    auto recv_msg_shutdown1 = sender.recv();
    REQUIRE(recv_msg_shutdown1.getVerb().first == CSCP1Message::Type::INVALID);
    REQUIRE_THAT(to_string(recv_msg_shutdown1.getVerb().second),
                 Equals("Satellite cannot be shut down from current state ORBIT"));

    // Send land
    auto land_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "land"});
    sender.send(land_msg);
    auto recv_msg_land = sender.recv();
    REQUIRE(recv_msg_land.getVerb().first == CSCP1Message::Type::SUCCESS);
    std::this_thread::sleep_for(150ms);

    // Try shutdown & succeed
    auto shutdown2_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "shutdown"});
    sender.send(shutdown2_msg);
    auto recv_msg_shutdown2 = sender.recv();
    REQUIRE(recv_msg_shutdown2.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_shutdown2.getVerb().second), Equals("Shutting down satellite"));
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
    initialize_msg.addPayload({"dummy_payload"s});
    sender.send(initialize_msg);

    // Check reply
    auto recv_msg_initialize = sender.recv();
    REQUIRE(recv_msg_initialize.getVerb().first == CSCP1Message::Type::INCOMPLETE);
    REQUIRE_THAT(to_string(recv_msg_initialize.getVerb().second),
                 Equals("Transition initialize received incorrect payload"));

    // Check state
    std::this_thread::sleep_for(150ms);
    sender.sendCommand("get_state");
    auto recv_msg_get_status = sender.recv();
    REQUIRE(recv_msg_get_status.getVerb().first == CSCP1Message::Type::SUCCESS);
    REQUIRE_THAT(to_string(recv_msg_get_status.getVerb().second), Equals("NEW"));
}

TEST_CASE("Catch invalid user command registrations", "[satellite]") {
    class MySatellite : public DummySatellite {
        // NOLINTNEXTLINE(readability-convert-member-functions-to-static,readability-make-member-function-const)
        int cmd() { return 2; }

    public:
        MySatellite() { register_command("", "A User Command", {}, &MySatellite::cmd, this); }
    };
    REQUIRE_THROWS_MATCHES(std::make_shared<MySatellite>(), LogicError, Message("Command name is invalid"));

    class MySatelliteI : public DummySatellite {
        // NOLINTNEXTLINE(readability-convert-member-functions-to-static,readability-make-member-function-const)
        int cmd() { return 2; }

    public:
        MySatelliteI() { register_command("command_with_amper&sand", "A User Command", {}, &MySatelliteI::cmd, this); }
    };
    REQUIRE_THROWS_MATCHES(std::make_shared<MySatelliteI>(), LogicError, Message("Command name is invalid"));

    class MySatellite2 : public DummySatellite {
        // NOLINTNEXTLINE(readability-convert-member-functions-to-static,readability-make-member-function-const)
        int cmd() { return 2; }

    public:
        MySatellite2() {
            register_command("my_cmd", "A User Command", {}, &MySatellite2::cmd, this);
            register_command("my_cmd", "A User Command", {}, &MySatellite2::cmd, this);
        }
    };
    REQUIRE_THROWS_MATCHES(
        std::make_shared<MySatellite2>(), LogicError, Message("Command \"my_cmd\" is already registered"));

    class MySatellite3 : public DummySatellite {
        // NOLINTNEXTLINE(readability-convert-member-functions-to-static,readability-make-member-function-const)
        int cmd() { return 2; }

    public:
        MySatellite3() { register_command("initialize", "A User Command", {}, &MySatellite3::cmd, this); }
    };
    REQUIRE_THROWS_MATCHES(
        std::make_shared<MySatellite3>(), LogicError, Message("Satellite transition command with this name exists"));

    class MySatellite4 : public DummySatellite {
        // NOLINTNEXTLINE(readability-convert-member-functions-to-static,readability-make-member-function-const)
        int cmd() { return 2; }

    public:
        MySatellite4() { register_command("get_commands", "A User Command", {}, &MySatellite4::cmd, this); }
    };
    REQUIRE_THROWS_MATCHES(
        std::make_shared<MySatellite4>(), LogicError, Message("Standard satellite command with this name exists"));
}

TEST_CASE("Catch incorrect user command arguments", "[satellite]") {
    // Create and start satellite
    auto satellite = std::make_shared<DummySatellite>();
    auto satellite_implementation = SatelliteImplementation(satellite);
    satellite_implementation.start();

    // Create sender
    CSCPSender sender {satellite_implementation.getPort()};

    // my_usr_cmd_arg with wrong payload encoding
    auto nolist_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "my_cmd_arg"});
    auto nolist_payload = PayloadBuffer("dummy payload"s);
    nolist_msg.addPayload(std::move(nolist_payload));
    sender.send(nolist_msg);

    auto recv_msg_nolist = sender.recv();
    REQUIRE(recv_msg_nolist.getVerb().first == CSCP1Message::Type::INCOMPLETE);
    REQUIRE_THAT(to_string(recv_msg_nolist.getVerb().second), Equals("Could not convert command payload to argument list"));

    // my_usr_cmd_arg with wrong argument type
    auto wrongarg_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "my_cmd_arg"});
    List args {};
    args.push_back(std::chrono::system_clock::now());
    wrongarg_msg.addPayload(args.assemble());
    sender.send(wrongarg_msg);

    auto recv_msg_wrongarg = sender.recv();
    REQUIRE(recv_msg_wrongarg.getVerb().first == CSCP1Message::Type::INCOMPLETE);
    REQUIRE_THAT(to_string(recv_msg_wrongarg.getVerb().second),
                 StartsWith("Mismatch of argument type \"int\" to provided type \"std::chrono::time_point"));

    // my_usr_cmd_arg with wrong number of argument
    auto manyarg_msg = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "my_cmd_arg"});
    List manyargs {};
    manyargs.push_back(3);
    manyargs.push_back(4);
    manyarg_msg.addPayload(manyargs.assemble());
    sender.send(manyarg_msg);

    auto recv_msg_manyarg = sender.recv();
    REQUIRE(recv_msg_manyarg.getVerb().first == CSCP1Message::Type::INCOMPLETE);
    REQUIRE_THAT(to_string(recv_msg_manyarg.getVerb().second),
                 Equals("Command \"my_cmd_arg\" expects 1 arguments but 2 given"));

    // my_usr_state from wrong state
    sender.sendCommand("my_cmd_state");
    auto recv_msg_usr_cmd_state = sender.recv();
    REQUIRE(recv_msg_usr_cmd_state.getVerb().first == CSCP1Message::Type::INVALID);
    REQUIRE_THAT(to_string(recv_msg_usr_cmd_state.getVerb().second),
                 Equals("Command my_cmd_state cannot be called in state NEW"));
    REQUIRE(!recv_msg_usr_cmd_state.hasPayload());
}

TEST_CASE("Catch incorrect user command return value", "[satellite]") {
    // Create and start satellite
    auto satellite = std::make_shared<DummySatellite>();
    auto satellite_implementation = SatelliteImplementation(satellite);
    satellite_implementation.start();

    // Create sender
    CSCPSender sender {satellite_implementation.getPort()};

    // my_usr_cmd_arg with wrong payload encoding
    auto invalid_return = CSCP1Message({"cscp_sender"}, {CSCP1Message::Type::REQUEST, "my_cmd_invalid_return"});
    sender.send(invalid_return);

    auto recv_msg_invalid_return = sender.recv();
    REQUIRE(recv_msg_invalid_return.getVerb().first == CSCP1Message::Type::INCOMPLETE);
    REQUIRE_THAT(to_string(recv_msg_invalid_return.getVerb().second),
                 Equals("Error casting function return type \"std::array<int, 1ul>\" to dictionary value"));
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
