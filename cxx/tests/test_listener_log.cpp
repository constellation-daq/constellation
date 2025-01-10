/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string_view>

#include <catch2/catch_test_macros.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/listener/LogListener.hpp"

#include "chirp_mock.hpp"
#include "cmdp_mock.hpp"

using namespace constellation;
using namespace constellation::listener;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::utils;

namespace {
    bool check_sub_message(zmq::message_t msg, bool subscribe, std::string_view topic) {
        // First byte is subscribe bool
        const auto msg_subscribe = static_cast<bool>(*msg.data<uint8_t>());
        if(msg_subscribe != subscribe) {
            return false;
        }
        // Rest is subscription topic
        auto msg_topic = msg.to_string_view();
        msg_topic.remove_prefix(1);
        return msg_topic == topic;
    }
} // namespace

TEST_CASE("Changing subscriptions", "[core][core::pools]") {
    // Create CHIRP manager for monitoring service discovery
    auto chirp_manager = create_chirp_manager();

    // Start pool
    auto pool = LogListener("pool", {});
    pool.startPool();

    // Set subscription topics
    pool.setSubscriptionTopics({"LOG/STATUS", "LOG/INFO"});

    // Start the sender and mock via chirp
    auto sender = CMDPSender("CMDPSender.s1");
    chirp_mock_service(sender.getName(), chirp::MONITORING, sender.getPort());

    // Check subscription messages
    REQUIRE(sender.canRecv());
    REQUIRE(sender.canRecv());

    // Unsubscribe from topic
    pool.unsubscribe("LOG/INFO");
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/INFO"));

    // No non-subscribed unsubscriptions
    pool.unsubscribe("LOG/INFO");
    pool.unsubscribe("LOG/NOTSUBSCRIBED");
    REQUIRE_FALSE(sender.canRecv());

    // Subscribe to new topic
    pool.subscribe("LOG/TRACE");
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/TRACE"));

    // No duplicate subscriptions
    pool.subscribe("LOG/TRACE");
    REQUIRE_FALSE(sender.canRecv());
}

TEST_CASE("Changing extra subscriptions", "[core][core::pools]") {
    // Create CHIRP manager for monitoring service discovery
    auto chirp_manager = create_chirp_manager();

    // Start pool
    auto pool = LogListener("pool", {});
    pool.startPool();

    // Set subscription topics
    pool.setSubscriptionTopics({"LOG/STATUS", "LOG/INFO"});

    // Start the senders and mock via chirp
    auto sender1 = CMDPSender("CMDPSender.s1");
    chirp_mock_service(sender1.getName(), chirp::MONITORING, sender1.getPort());
    auto sender2 = CMDPSender("CMDPSender.s2");
    chirp_mock_service(sender2.getName(), chirp::MONITORING, sender2.getPort());

    // Pop subscription messages
    REQUIRE(sender1.canRecv());
    REQUIRE(sender1.canRecv());
    REQUIRE(sender2.canRecv());
    REQUIRE(sender2.canRecv());

    // Add extra subscription: s1 now at LOG/STATUS, LOG/INFO, LOG/TRACE
    pool.subscribeExtra(to_string(sender1.getName()), "LOG/TRACE");

    // Check subscription message
    REQUIRE(check_sub_message(sender1.recv().pop(), true, "LOG/TRACE"));

    // No duplicate extra subscriptions
    pool.subscribeExtra(to_string(sender1.getName()), "LOG/TRACE");
    REQUIRE_FALSE(sender1.canRecv());

    // Replace extra subscription: s1 now at LOG/STATUS, LOG/INFO, LOG/DEBUG
    pool.setExtraSubscriptionTopics(to_string(sender1.getName()), {"LOG/DEBUG", "LOG/INFO"});

    // Check changing subscriptions
    REQUIRE(check_sub_message(sender1.recv().pop(), false, "LOG/TRACE"));
    REQUIRE(check_sub_message(sender1.recv().pop(), true, "LOG/DEBUG"));

    // Unsubscribe from LOG/INFO for all
    pool.unsubscribe("LOG/INFO");
    REQUIRE(check_sub_message(sender1.recv().pop(), false, "LOG/INFO"));
    REQUIRE(check_sub_message(sender2.recv().pop(), false, "LOG/INFO"));

    // Check that sender1 gets subscription again since extra topic
    REQUIRE(check_sub_message(sender1.recv().pop(), true, "LOG/INFO"));

    // Remove extra subscriptions
    pool.removeExtraSubscriptions(to_string(sender1.getName()));
    REQUIRE(check_sub_message(sender1.recv().pop(), false, "LOG/DEBUG"));
}

TEST_CASE("Extra subscriptions on connection", "[core][core::pools]") {
    // Create CHIRP manager for monitoring service discovery
    auto chirp_manager = create_chirp_manager();

    // Start pool
    auto pool = LogListener("pool", {});
    pool.startPool();

    // Set subscription topics
    pool.setSubscriptionTopics({"LOG/STATUS", "LOG/INFO"});
    pool.setExtraSubscriptionTopics("CMDPSender.s1", {"LOG/INFO", "SOMETHING", "ELSE"});
    pool.unsubscribeExtra("CMDPSender.s1", "ELSE");

    // Start the senders and mock via chirp
    auto sender = CMDPSender("CMDPSender.s1");
    chirp_mock_service(sender.getName(), chirp::MONITORING, sender.getPort());

    // Pop subscription messages for global subscriptions
    REQUIRE(sender.canRecv());
    REQUIRE(sender.canRecv());

    // Check extra subscription message
    REQUIRE(check_sub_message(sender.recv().pop(), true, "SOMETHING"));

    // Remove all extra subscriptions
    pool.removeExtraSubscriptions();

    // Check unsubscription message
    REQUIRE(check_sub_message(sender.recv().pop(), false, "SOMETHING"));
}
