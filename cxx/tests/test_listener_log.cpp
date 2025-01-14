/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <map>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/listener/LogListener.hpp"

#include "chirp_mock.hpp"
#include "cmdp_mock.hpp"

using namespace Catch::Matchers;
using namespace constellation;
using namespace constellation::listener;
using namespace constellation::log;
using namespace constellation::utils;

TEST_CASE("Global log level", "[core][core::pools]") {
    // Create CHIRP manager for monitoring service discovery
    auto chirp_manager = create_chirp_manager();

    // Start pool
    auto listener = LogListener("listener", {});
    listener.startPool();

    // Set global log subscription level
    listener.setGlobalLogLevel(Level::INFO);

    // Start the sender and mock via chirp
    auto sender = CMDPSender("CMDPSender.s1");
    chirp_mock_service(sender.getName(), chirp::MONITORING, sender.getPort());

    // Pop subscription messages (INFO, WARNING, STATUS, CRITICAL)
    REQUIRE(sender.canRecv());
    REQUIRE(sender.canRecv());
    REQUIRE(sender.canRecv());
    REQUIRE(sender.canRecv());

    // Check global subscription is not returned in topic subscriptions
    REQUIRE_THAT(listener.getLogTopicSubscriptions(), RangeEquals(std::map<std::string, Level>({})));
    REQUIRE(listener.getGlobalLogLevel() == Level::INFO);

    // Reduce global level
    listener.setGlobalLogLevel(Level::TRACE);
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/TRACE"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/DEBUG"));

    // Increase global level
    listener.setGlobalLogLevel(Level::STATUS);
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/TRACE"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/DEBUG"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/INFO"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/WARNING"));

    // Turn off global subscription
    listener.setGlobalLogLevel(Level::OFF);
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/STATUS"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/CRITICAL"));
}

TEST_CASE("Topic subscriptions", "[core][core::pools]") {
    // Create CHIRP manager for monitoring service discovery
    auto chirp_manager = create_chirp_manager();

    // Start pool
    auto listener = LogListener("listener", {});
    listener.startPool();

    // Subscribe to topic
    listener.subscribeLogTopic("FSM", Level::INFO);

    // Start the sender and mock via chirp
    auto sender = CMDPSender("CMDPSender.s1");
    chirp_mock_service(sender.getName(), chirp::MONITORING, sender.getPort());

    // Pop subscription messages (INFO, WARNING, STATUS, CRITICAL)
    REQUIRE(sender.canRecv());
    REQUIRE(sender.canRecv());
    REQUIRE(sender.canRecv());
    REQUIRE(sender.canRecv());

    // Subscribe to new topic
    listener.subscribeLogTopic("SATELLITE", Level::WARNING);
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/WARNING/SATELLITE"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/STATUS/SATELLITE"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/CRITICAL/SATELLITE"));

    // Check subscribed topics
    REQUIRE_THAT(listener.getLogTopicSubscriptions(),
                 RangeEquals(std::map<std::string, Level>({{"FSM", Level::INFO}, {"SATELLITE", Level::WARNING}})));

    // Unsubscribe from a topic
    listener.unsubscribeLogTopic("SATELLITE");
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/WARNING/SATELLITE"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/STATUS/SATELLITE"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/CRITICAL/SATELLITE"));

    // Check subscribed topics again
    REQUIRE_THAT(listener.getLogTopicSubscriptions(), RangeEquals(std::map<std::string, Level>({{"FSM", Level::INFO}})));
}

TEST_CASE("Extra topic subscriptions", "[core][core::pools]") {
    // Create CHIRP manager for monitoring service discovery
    auto chirp_manager = create_chirp_manager();

    // Start pool
    auto listener = LogListener("listener", {});
    listener.startPool();

    // Start the sender and mock via chirp
    auto sender = CMDPSender("CMDPSender.s1");
    chirp_mock_service(sender.getName(), chirp::MONITORING, sender.getPort());

    // Subscribe to extra topic
    listener.subscribeExtaLogTopic(to_string(sender.getName()), "FSM", Level::INFO);

    // Check subscription messages
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/INFO/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/WARNING/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/STATUS/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/CRITICAL/FSM"));

    // Check extra log topic subscriptions
    REQUIRE_THAT(listener.getExtraLogTopicSubscriptions(to_string(sender.getName())),
                 RangeEquals(std::map<std::string, Level>({{"FSM", Level::INFO}})));

    // Unsubscribe from extra topic
    listener.unsubscribeExtraLogTopic(to_string(sender.getName()), "FSM");
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/INFO/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/WARNING/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/STATUS/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/CRITICAL/FSM"));
}
