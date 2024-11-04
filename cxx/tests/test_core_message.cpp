/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <cstdint>
#include <limits>
#include <numbers>
#include <string>
#include <utility>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <msgpack.hpp>
#include <zmq.hpp>

#include "constellation/core/log/Level.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/string.hpp"

using namespace Catch::Matchers;
using namespace constellation::config;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::utils;
using namespace std::string_literals;

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Basic Header Functions", "[core][core::message]") {
    auto tp = std::chrono::system_clock::now();

    const CSCP1Message::Header cscp1_header {"senderCSCP", tp};

    REQUIRE_THAT(to_string(cscp1_header.getSender()), Equals("senderCSCP"));
    REQUIRE(cscp1_header.getTime() == tp);
    REQUIRE(cscp1_header.getTags().empty());
    REQUIRE_THAT(cscp1_header.to_string(), ContainsSubstring("CSCP1"));
}

TEST_CASE("Basic Header Functions (CDTP1)", "[core][core::message]") {
    auto tp = std::chrono::system_clock::now();

    const CDTP1Message::Header cdtp1_header {"senderCDTP", 0, CDTP1Message::Type::BOR, tp};

    REQUIRE_THAT(to_string(cdtp1_header.getSender()), Equals("senderCDTP"));
    REQUIRE(cdtp1_header.getType() == CDTP1Message::Type::BOR);
    REQUIRE(cdtp1_header.getTime() == tp);
    REQUIRE(cdtp1_header.getTags().empty());
    REQUIRE_THAT(cdtp1_header.to_string(), ContainsSubstring("CDTP1"));
}

TEST_CASE("Header String Output", "[core][core::message]") {
    // Get fixed timepoint (unix epoch)
    const std::chrono::system_clock::time_point tp {};

    CMDP1Message::Header cmdp1_header {"senderCMDP", tp};

    cmdp1_header.setTag("test_b", true);
    cmdp1_header.setTag("test_i", 7);
    cmdp1_header.setTag("test_d", 1.5);
    cmdp1_header.setTag("test_s", "String"s);
    cmdp1_header.setTag("test_t", tp);

    const auto string_out = cmdp1_header.to_string();

    REQUIRE_THAT(string_out, ContainsSubstring("Header: CMDP1"));
    REQUIRE_THAT(string_out, ContainsSubstring("Sender: senderCMDP"));
    REQUIRE_THAT(string_out, ContainsSubstring("Time:   1970-01-01 00:00:00.000000"));
    REQUIRE_THAT(string_out, ContainsSubstring("test_b: true"));
    REQUIRE_THAT(string_out, ContainsSubstring("test_i: 7"));
    REQUIRE_THAT(string_out, ContainsSubstring("test_d: 1.5"));
    REQUIRE_THAT(string_out, ContainsSubstring("test_s: String"));
    REQUIRE_THAT(string_out, ContainsSubstring("test_t: 1970-01-01 00:00:00.000000"));
}

TEST_CASE("Header String Output (CDTP1)", "[core][core::message]") {
    const CDTP1Message::Header cdtp1_header {"senderCMDP", 1234, CDTP1Message::Type::DATA};

    const auto string_out = cdtp1_header.to_string();

    REQUIRE_THAT(string_out, ContainsSubstring("Type:   DATA"));
    REQUIRE_THAT(string_out, ContainsSubstring("Seq No: 1234"));
}

TEST_CASE("Header Packing / Unpacking", "[core][core::message]") {
    auto tp = std::chrono::system_clock::now();

    CSCP1Message::Header cscp1_header {"senderCSCP", tp};

    cscp1_header.setTag("test_b", true);
    cscp1_header.setTag("test_i", std::numeric_limits<std::int64_t>::max());
    cscp1_header.setTag("test_d", std::numbers::pi);
    cscp1_header.setTag("test_s", "String"s);
    cscp1_header.setTag("test_t", tp);
    cscp1_header.setTag("Test_C", 0);

    // Pack header
    msgpack::sbuffer sbuf {};
    msgpack::pack(sbuf, cscp1_header);

    // Unpack header
    const auto cscp1_header_unpacked = CSCP1Message::Header::disassemble({to_byte_ptr(sbuf.data()), sbuf.size()});

    // Compare unpacked header
    REQUIRE(cscp1_header_unpacked.getTags().size() == 6);
    REQUIRE(cscp1_header_unpacked.getTag<bool>("test_b"));
    REQUIRE(cscp1_header_unpacked.getTag<std::int64_t>("test_i") == std::numeric_limits<std::int64_t>::max());
    REQUIRE(cscp1_header_unpacked.getTag<double>("test_d") == std::numbers::pi);
    REQUIRE_THAT(cscp1_header_unpacked.getTag<std::string>("test_s"), Equals("String"));
    REQUIRE(cscp1_header_unpacked.getTag<std::chrono::system_clock::time_point>("test_t") == tp);
    REQUIRE(cscp1_header_unpacked.hasTag("tEst_C"));
    REQUIRE(cscp1_header_unpacked.getTag<int>("teSt_c") == 0);
}

TEST_CASE("Header Packing / Unpacking (invalid protocol)", "[core][core::message]") {
    const CSCP1Message::Header cscp1_header {"senderCSCP"};

    // Pack header
    msgpack::sbuffer sbuf {};
    // first pack version
    msgpack::pack(sbuf, "INVALID");
    // then sender
    msgpack::pack(sbuf, "SenderCSCP");
    // then time
    msgpack::pack(sbuf, std::chrono::system_clock::now());
    // then tags
    msgpack::pack(sbuf, Dictionary());

    // Check for wrong protocol to be picked up
    REQUIRE_THROWS_MATCHES(CMDP1Message::Header::disassemble({to_byte_ptr(sbuf.data()), sbuf.size()}),
                           InvalidProtocolError,
                           Message("Invalid protocol identifier \"INVALID\""));
    // CDTP1 has separate header implementation, also test this:
    REQUIRE_THROWS_MATCHES(CDTP1Message::Header::disassemble({to_byte_ptr(sbuf.data()), sbuf.size()}),
                           InvalidProtocolError,
                           Message("Invalid protocol identifier \"INVALID\""));
}

TEST_CASE("Header Packing / Unpacking (unexpected protocol)", "[core][core::message]") {
    const CSCP1Message::Header cscp1_header {"senderCSCP"};

    // Pack header
    msgpack::sbuffer sbuf {};
    msgpack::pack(sbuf, cscp1_header);

    // Check for wrong protocol to be picked up
    REQUIRE_THROWS_MATCHES(CMDP1Message::Header::disassemble({to_byte_ptr(sbuf.data()), sbuf.size()}),
                           UnexpectedProtocolError,
                           Message("Received protocol \"CSCP1\" does not match expected identifier \"CMDP1\""));
    // CDTP1 has separate header implementation, also test this:
    REQUIRE_THROWS_MATCHES(CDTP1Message::Header::disassemble({to_byte_ptr(sbuf.data()), sbuf.size()}),
                           UnexpectedProtocolError,
                           Message("Received protocol \"CSCP1\" does not match expected identifier \"CDTP1\""));
}

TEST_CASE("Message Assembly / Disassembly (CMDP1)", "[core][core::message]") {
    // Log message with logger topic
    CMDP1LogMessage log_msg {Level::STATUS, "Logger_Topic", {"senderCMDP"}, "log message"};
    auto log_frames = log_msg.assemble();

    auto log_msg2_raw = CMDP1Message::disassemble(log_frames);
    REQUIRE(log_msg2_raw.isLogMessage());
    REQUIRE_THAT(to_string(log_msg2_raw.getTopic()), Equals("LOG/STATUS/LOGGER_TOPIC"));

    const auto log_msg2 = CMDP1LogMessage(std::move(log_msg2_raw));
    REQUIRE_THAT(log_msg2.getHeader().to_string(), ContainsSubstring("Sender: senderCMDP"));
    REQUIRE(log_msg2.isLogMessage());
    REQUIRE(log_msg2.getLogLevel() == Level::STATUS);
    REQUIRE_THAT(to_string(log_msg2.getLogTopic()), Equals("LOGGER_TOPIC"));
    REQUIRE_THAT(to_string(log_msg2.getLogMessage()), Equals("log message"));

    // Log message without logger topic (default logger)
    CMDP1LogMessage dl_log_msg {Level::STATUS, "", {"senderCMDP"}, "log message"};
    auto dl_log_frames = dl_log_msg.assemble();

    auto dl_log_msg2 = CMDP1LogMessage::disassemble(dl_log_frames);
    REQUIRE_THAT(to_string(dl_log_msg2.getLogTopic()), Equals(""));
}

TEST_CASE("Message Assembly / Disassembly (CMDP1, invalid number of frames)", "[core][core::message]") {
    CMDP1LogMessage log_msg {Level::STATUS, "", {"senderCMDP"}, ""};
    auto log_frames = log_msg.assemble();

    // Add invalid fourth frame
    log_frames.addstr("should not be here");

    REQUIRE_THROWS_MATCHES(CMDP1Message::disassemble(log_frames),
                           MessageDecodingError,
                           Message("Error decoding message: Invalid number of message frames"));
}

TEST_CASE("Message Assembly / Disassembly (CMDP1, invalid topic)", "[core][core::message]") {
    CMDP1LogMessage log_msg {Level::STATUS, "", {"senderCMDP"}, ""};
    auto log_frames = log_msg.assemble();

    // Add invalid fourth frame
    zmq::message_t invalid_topic {"INVALID/TOPIC"s};
    log_frames.at(0).swap(invalid_topic);

    REQUIRE_THROWS_MATCHES(CMDP1Message::disassemble(log_frames),
                           MessageDecodingError,
                           Message("Error decoding message: Invalid message topic, neither log or statistics message"));
}

TEST_CASE("Message Assembly / Disassembly (CMDP1, invalid log level)", "[core][core::message]") {
    CMDP1LogMessage log_msg {Level::STATUS, "", {"senderCMDP"}, ""};
    auto log_frames = log_msg.assemble();

    // Add invalid fourth frame
    zmq::message_t invalid_topic {"LOG/ERROR"s};
    log_frames.at(0).swap(invalid_topic);

    REQUIRE_THROWS_MATCHES(CMDP1Message::disassemble(log_frames),
                           MessageDecodingError,
                           Message("Error decoding message: \"ERROR\" is not a valid log level"));
}

TEST_CASE("Message Assembly / Disassembly (CSCP1)", "[core][core::message]") {
    auto tp = std::chrono::system_clock::now();

    CSCP1Message cscp1_msg {{"senderCSCP", tp}, {CSCP1Message::Type::SUCCESS, ""}};
    auto frames = cscp1_msg.assemble();

    auto cscp1_msg2 = CSCP1Message::disassemble(frames);

    REQUIRE_THAT(cscp1_msg2.getHeader().to_string(), ContainsSubstring("Sender: senderCSCP"));
    REQUIRE(cscp1_msg2.getVerb().first == CSCP1Message::Type::SUCCESS);
}

TEST_CASE("Message Assembly / Disassembly (CDTP1)", "[core][core::message]") {
    CDTP1Message cdtp1_msg {{"senderCDTP", 1234, CDTP1Message::Type::DATA}, 1};
    REQUIRE(cdtp1_msg.getPayload().empty());

    auto frames = cdtp1_msg.assemble();
    auto cdtp1_msg2 = CDTP1Message::disassemble(frames);

    REQUIRE_THAT(cdtp1_msg2.getHeader().to_string(), ContainsSubstring("Sender: senderCDTP"));
    REQUIRE(cdtp1_msg2.getPayload().empty());
}

TEST_CASE("Message Assembly / Disassembly (CDTP1, wrong number of frames)", "[core][core::message]") {
    CDTP1Message cdtp1_msg {{"senderCDTP", 1234, CDTP1Message::Type::BOR}, 2};
    cdtp1_msg.addPayload("frame1"s);
    cdtp1_msg.addPayload("frame2"s);

    auto frames = cdtp1_msg.assemble();

    REQUIRE_THROWS_MATCHES(
        CDTP1Message::disassemble(frames),
        MessageDecodingError,
        Message("Error decoding message: Wrong number of frames for BOR, exactly one payload frame expected"));
}

TEST_CASE("Incorrect message type (CMDP1)", "[core][core::message]") {
    auto tp = std::chrono::system_clock::now();

    // Log message with logger topic
    CMDP1LogMessage log_msg {Level::STATUS, "logger", {"senderCMDP", tp}, ""};
    auto log_frames = log_msg.assemble();

    // Actually a stat message TODO(stephan.lachnit): use CMDPStatMessage once implemented
    zmq::message_t stat_topic {"STAT/STATI_TOPIC"s};
    log_frames.at(0).swap(stat_topic);
    REQUIRE_THROWS_MATCHES(CMDP1LogMessage::disassemble(log_frames),
                           IncorrectMessageType,
                           Message("Message type is incorrect: Not a log message"));
}

TEST_CASE("Message Payload (CSCP1)", "[core][core::message]") {
    auto tp = std::chrono::system_clock::now();

    CSCP1Message cscp1_msg {{"senderCSCP", tp}, {CSCP1Message::Type::SUCCESS, ""}};
    REQUIRE(cscp1_msg.getPayload().empty());

    // Add payload frame
    msgpack::sbuffer sbuf_header {};
    msgpack::pack(sbuf_header, "this is fine");
    cscp1_msg.addPayload(std::move(sbuf_header));

    // Assemble and disassemble message
    auto frames = cscp1_msg.assemble();
    auto cscp1_msg2 = CSCP1Message::disassemble(frames);

    // Retrieve payload
    const auto& data = cscp1_msg2.getPayload();
    const auto py_string = msgpack::unpack(to_char_ptr(data.span().data()), data.span().size());
    REQUIRE_THAT(py_string->as<std::string>(), Equals("this is fine"));
}

TEST_CASE("Message Payload (CSCP1, too many frames)", "[core][core::message]") {
    auto tp = std::chrono::system_clock::now();

    CSCP1Message cscp1_message {{"senderCSCP", tp}, {CSCP1Message::Type::SUCCESS, ""}};
    auto frames = cscp1_message.assemble();

    // Attach additional frames:
    msgpack::sbuffer sbuf_header {};
    msgpack::pack(sbuf_header, "this is fine");
    auto payload = PayloadBuffer(std::move(sbuf_header));
    frames.add(payload.to_zmq_msg_copy());
    frames.add(payload.to_zmq_msg_release());

    // Check for excess frame detection
    REQUIRE_THROWS_MATCHES(CSCP1Message::disassemble(frames),
                           MessageDecodingError,
                           Message("Error decoding message: Incorrect number of message frames"));
}

TEST_CASE("Message Payload (CDTP1)", "[core][core::message]") {
    auto tp = std::chrono::system_clock::now();

    CDTP1Message cdtp1_msg {{"senderCDTP", 1234, CDTP1Message::Type::DATA, tp}, 3};

    // Add payload frame
    for(int i = 0; i < 3; i++) {
        msgpack::sbuffer sbuf_header {};
        msgpack::pack(sbuf_header, "this is fine");
        cdtp1_msg.addPayload(std::move(sbuf_header));
    }

    // Assemble and disassemble message
    auto frames = cdtp1_msg.assemble();
    auto cdtp1_msg2 = CDTP1Message::disassemble(frames);

    // Retrieve payload
    const auto& data = cdtp1_msg2.getPayload();
    REQUIRE(data.size() == 3);

    const auto front_span = data.front().span();
    const auto py_string = msgpack::unpack(to_char_ptr(front_span.data()), front_span.size());
    REQUIRE_THAT(py_string->as<std::string>(), Equals("this is fine"));
}

TEST_CASE("Packing / Unpacking (CDTP1)", "[core][core::message]") {
    constexpr std::uint64_t seq_no = 1234;
    const CDTP1Message::Header cdtp1_header {"senderCDTP", seq_no, CDTP1Message::Type::EOR};

    // Pack header
    msgpack::sbuffer sbuf {};
    msgpack::pack(sbuf, cdtp1_header);

    // Unpack header
    const auto cdtp1_header_unpacked = CDTP1Message::Header::disassemble({to_byte_ptr(sbuf.data()), sbuf.size()});

    // Compare unpacked header
    REQUIRE(cdtp1_header_unpacked.getType() == CDTP1Message::Type::EOR);
    REQUIRE(cdtp1_header_unpacked.getSequenceNumber() == seq_no);
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
