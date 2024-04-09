/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <ctime>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <msgpack.hpp>

#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/message/CMDP1Header.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/utils/casts.hpp"

using namespace Catch::Matchers;
using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::utils;
using namespace std::literals::string_literals;

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
    auto tp = std::chrono::system_clock::from_time_t(std::time_t(0));

    CMDP1Header cmdp1_header {"senderCMDP", tp};

    cmdp1_header.setTag("test_b", true);
    cmdp1_header.setTag("test_i", 7);
    cmdp1_header.setTag("test_d", 1.5);
    cmdp1_header.setTag("test_s", "String"s);
    cmdp1_header.setTag("test_t", tp);

    const auto string_out = cmdp1_header.to_string();

    REQUIRE_THAT(string_out, ContainsSubstring("Header: CMDP1"));
    REQUIRE_THAT(string_out, ContainsSubstring("Sender: senderCMDP"));
    REQUIRE_THAT(string_out, ContainsSubstring("Time:   1970-01-01 00:00:00.000000000"));
    REQUIRE_THAT(string_out, ContainsSubstring("test_b: true"));
    REQUIRE_THAT(string_out, ContainsSubstring("test_i: 7"));
    REQUIRE_THAT(string_out, ContainsSubstring("test_d: 1.5"));
    REQUIRE_THAT(string_out, ContainsSubstring("test_s: String"));
    REQUIRE_THAT(string_out, ContainsSubstring("test_t: 1970-01-01 00:00:00.000000000"));
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

    // Pack header
    msgpack::sbuffer sbuf {};
    msgpack::pack(sbuf, cscp1_header);

    // Unpack header
    const auto cscp1_header_unpacked = CSCP1Message::Header::disassemble({to_byte_ptr(sbuf.data()), sbuf.size()});

    // Compare unpacked header
    REQUIRE(cscp1_header_unpacked.getTags().size() == 5);
    REQUIRE(std::get<bool>(cscp1_header_unpacked.getTag("test_b")));
    REQUIRE(std::get<std::int64_t>(cscp1_header_unpacked.getTag("test_i")) == std::numeric_limits<std::int64_t>::max());
    REQUIRE(std::get<double>(cscp1_header_unpacked.getTag("test_d")) == std::numbers::pi);
    REQUIRE_THAT(std::get<std::string>(cscp1_header_unpacked.getTag("test_s")), Equals("String"));
    REQUIRE(std::get<std::chrono::system_clock::time_point>(cscp1_header_unpacked.getTag("test_t")) == tp);
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
    msgpack::pack(sbuf, Dictionary {});

    // Check for wrong protocol to be picked up
    REQUIRE_THROWS_AS(CMDP1Header::disassemble({to_byte_ptr(sbuf.data()), sbuf.size()}), InvalidProtocolError);
    REQUIRE_THROWS_MATCHES(CMDP1Header::disassemble({to_byte_ptr(sbuf.data()), sbuf.size()}),
                           InvalidProtocolError,
                           Message("Invalid protocol identifier \"INVALID\""));
    // CDTP1 has separate header implementation, also test this:
    REQUIRE_THROWS_AS(CDTP1Message::Header::disassemble({to_byte_ptr(sbuf.data()), sbuf.size()}), InvalidProtocolError);
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
    REQUIRE_THROWS_AS(CMDP1Header::disassemble({to_byte_ptr(sbuf.data()), sbuf.size()}), UnexpectedProtocolError);
    REQUIRE_THROWS_MATCHES(CMDP1Header::disassemble({to_byte_ptr(sbuf.data()), sbuf.size()}),
                           UnexpectedProtocolError,
                           Message("Received protocol \"CSCP1\" does not match expected identifier \"CMDP1\""));
    // CDTP1 has separate header implementation, also test this:
    REQUIRE_THROWS_AS(CDTP1Message::Header::disassemble({to_byte_ptr(sbuf.data()), sbuf.size()}), UnexpectedProtocolError);
    REQUIRE_THROWS_MATCHES(CDTP1Message::Header::disassemble({to_byte_ptr(sbuf.data()), sbuf.size()}),
                           UnexpectedProtocolError,
                           Message("Received protocol \"CSCP1\" does not match expected identifier \"CDTP1\""));
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

TEST_CASE("Message Payload (CSCP1)", "[core][core::message]") {
    auto tp = std::chrono::system_clock::now();

    CSCP1Message cscp1_msg {{"senderCSCP", tp}, {CSCP1Message::Type::SUCCESS, ""}};
    REQUIRE(cscp1_msg.getPayload() == nullptr);

    // Add payload frame
    msgpack::sbuffer sbuf_header {};
    msgpack::pack(sbuf_header, "this is fine");
    auto payload = std::make_shared<zmq::message_t>(sbuf_header.data(), sbuf_header.size());
    cscp1_msg.addPayload(std::move(payload));

    // Assemble and disassemble message
    auto frames = cscp1_msg.assemble();
    auto cscp1_msg2 = CSCP1Message::disassemble(frames);

    // Retrieve payload
    auto data = cscp1_msg2.getPayload();
    auto py_string = msgpack::unpack(to_char_ptr(data->data()), data->size());
    REQUIRE_THAT(py_string->as<std::string>(), Equals("this is fine"));
}

TEST_CASE("Message Payload (CSCP1, too many frames)", "[core][core::message]") {
    auto tp = std::chrono::system_clock::now();

    CSCP1Message cscp1_message {{"senderCSCP", tp}, {CSCP1Message::Type::SUCCESS, ""}};
    auto frames = cscp1_message.assemble();

    // Attach additional frames:
    msgpack::sbuffer sbuf_header {};
    msgpack::pack(sbuf_header, "this is fine");
    frames.addmem(sbuf_header.data(), sbuf_header.size());
    frames.addmem(sbuf_header.data(), sbuf_header.size());

    // Check for excess frame detection
    REQUIRE_THROWS_AS(CSCP1Message::disassemble(frames), MessageDecodingError);
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
        auto payload = std::make_shared<zmq::message_t>(sbuf_header.data(), sbuf_header.size());
        cdtp1_msg.addPayload(std::move(payload));
    }

    // Assemble and disassemble message
    auto frames = cdtp1_msg.assemble();
    auto cdtp1_msg2 = CDTP1Message::disassemble(frames);

    // Retrieve payload
    auto data = cdtp1_msg2.getPayload();
    REQUIRE(data.size() == 3);

    auto py_string = msgpack::unpack(to_char_ptr(data.front()->data()), data.front()->size());
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
