/**
 * @file
 * @brief Tests for CHIRP broadcast receiver
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <cstddef>
#include <future>
#include <string> // IWYU pragma: keep
#include <vector>

#include <asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "constellation/core/chirp/BroadcastRecv.hpp"
#include "constellation/core/chirp/BroadcastSend.hpp"

using namespace Catch::Matchers;
using namespace constellation::chirp;
using namespace std::chrono_literals;
using namespace std::string_literals;

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Send and receive broadcast containing a string", "[chirp][broadcast]") {
    BroadcastRecv receiver {"0.0.0.0", 49152};
    BroadcastSend sender {"0.0.0.0", 49152};

    // Start receiving new message
    auto msg_future = std::async(&BroadcastRecv::recvBroadcast, &receiver);
    // Send message (string)
    auto msg_content = "test message"s;
    sender.sendBroadcast(msg_content);
    // Receive message
    auto msg = msg_future.get();

    REQUIRE_THAT(msg.to_string(), Equals(msg_content));
}

TEST_CASE("Send and receive broadcast containing binary content", "[chirp][broadcast]") {
    BroadcastRecv receiver {"0.0.0.0", 49152};
    BroadcastSend sender {"0.0.0.0", 49152};

    // Start receiving new message
    auto msg_future = std::async(&BroadcastRecv::recvBroadcast, &receiver);
    // Send message (bytes)
    auto msg_content = std::vector<std::byte>({std::byte('T'), std::byte('E'), std::byte('S'), std::byte('T')});
    sender.sendBroadcast(msg_content);
    // Receive message
    auto msg = msg_future.get();

    REQUIRE_THAT(msg.content, RangeEquals(msg_content));
}

TEST_CASE("Get IP address of broadcast from localhost", "[chirp][broadcast]") {
    BroadcastRecv receiver {"0.0.0.0", 49152};
    BroadcastSend sender {"0.0.0.0", 49152};

    // Start receiving new message
    auto msg_future = std::async(&BroadcastRecv::recvBroadcast, &receiver);
    // Send message
    auto msg_content = "test message"s;
    sender.sendBroadcast(msg_content);
    // Receive message
    auto msg = msg_future.get();

    REQUIRE(msg.address == asio::ip::make_address_v4("127.0.0.1"));
}

TEST_CASE("Send and receive broadcast asynchronously", "[chirp][broadcast]") {
    BroadcastRecv receiver {"0.0.0.0", 49152};
    BroadcastSend sender {"0.0.0.0", 49152};

    // Try receiving new message
    auto msg_opt_future = std::async(&BroadcastRecv::asyncRecvBroadcast, &receiver, 10ms);
    // Send message
    auto msg_content = "test message"s;
    sender.sendBroadcast(msg_content);
    // Receive message
    auto msg_opt = msg_opt_future.get();

    // Check that a message was received
    REQUIRE(msg_opt.has_value());

    // Get message
    const auto& msg = msg_opt.value(); // NOLINT(bugprone-unchecked-optional-access)

    REQUIRE_THAT(msg.to_string(), Equals(msg_content));
}

TEST_CASE("Get timeout on asynchronous broadcast receive", "[chirp][broadcast]") {
    BroadcastRecv receiver {"0.0.0.0", 49152};

    // Try receiving new message
    auto msg_opt = receiver.asyncRecvBroadcast(10ms);

    // No message send, thus check for timeout that there is no message
    REQUIRE_FALSE(msg_opt.has_value());
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
