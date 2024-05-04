/**
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <catch2/catch_test_macros.hpp>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/logging/log.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/core/logging/SinkManager.hpp"

using namespace constellation::log;

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Delayed first message", "[logging]") {
    // First message is delayed by 500ms, so call this here for better timing analysis
    SinkManager::getInstance().setCMDPLevelsCustom(TRACE);
    SinkManager::getInstance().setGlobalConsoleLevel(OFF);
    auto logger = Logger("DelayedFirstMessage");
    LOG(logger, TRACE) << "";
    SinkManager::getInstance().setCMDPLevelsCustom(OFF);
}

TEST_CASE("Default logger", "[logging]") {
    SinkManager::getInstance().setGlobalConsoleLevel(TRACE);
    LOG(Logger::getDefault(), STATUS) << "Message from default logger";
    // Default logger is not destructed and thus requires manual flushing
    Logger::getDefault().flush();
}

TEST_CASE("Basic logging", "[logging]") {
    auto logger = Logger("BasicLogging");

    SinkManager::getInstance().setGlobalConsoleLevel(TRACE);
    REQUIRE(logger.shouldLog(TRACE));

    LOG(logger, TRACE) << "trace"sv;
    LOG(logger, DEBUG) << "debug"sv;
    LOG(logger, INFO) << "info"sv;
    LOG(logger, STATUS) << "status"sv;
    LOG(logger, WARNING) << "warning"sv;
    LOG(logger, CRITICAL) << "critical"sv;
}

TEST_CASE("Logging macros", "[logging]") {
    auto logger = Logger("LoggingMacros");

    SinkManager::getInstance().setGlobalConsoleLevel(TRACE);

    int count_once {0};
    int count_n {0};
    int count_if {0};

    for(int i = 0; i < 5; ++i) {
        LOG_ONCE(logger, STATUS) << "log once, i="sv << i << ", count "sv << ++count_once;
        LOG_N(logger, STATUS, 3) << "log n, i="sv << i << ", count "sv << ++count_n;
        LOG_IF(logger, STATUS, i % 2 == 1) << "log if, i=" << i << ", count "sv << ++count_if;
    }

    REQUIRE(count_once == 1);
    REQUIRE(count_n == 3);
    REQUIRE(count_if == 2);
}

TEST_CASE("Log levels", "[logging]") {
    auto logger = Logger("LogLevels", INFO);

    SinkManager::getInstance().setGlobalConsoleLevel(STATUS);
    SinkManager::getInstance().setCMDPLevelsCustom(STATUS);

    REQUIRE_FALSE(logger.shouldLog(DEBUG));
    REQUIRE(logger.shouldLog(INFO));

    // Test global CMDP subscription
    SinkManager::getInstance().setCMDPLevelsCustom(DEBUG);
    REQUIRE(logger.shouldLog(DEBUG));

    // Test global CMDP unsubscription
    SinkManager::getInstance().setCMDPLevelsCustom(OFF);
    REQUIRE_FALSE(logger.shouldLog(DEBUG));

    // Test topic CMDP subscription
    SinkManager::getInstance().setCMDPLevelsCustom(STATUS, {{"LogLevels", DEBUG}});
    REQUIRE(logger.shouldLog(DEBUG));

    // Test topic CMDP subscription via matching
    SinkManager::getInstance().setCMDPLevelsCustom(STATUS, {{"LogLevels", DEBUG}, {"logle", TRACE}});
    REQUIRE(logger.shouldLog(TRACE));
}

TEST_CASE("Ephemeral CMDP port", "[logging]") {
    // Port number of ephemeral port should always be >=1024 on all OSes
    auto port_number = SinkManager::getInstance().getCMDPPort();
    REQUIRE(port_number >= 1024);
}

TEST_CASE("Register Service via CHIRP", "[logging]") {
    using namespace constellation::chirp;
    auto manager = Manager("255.255.255.255", "0.0.0.0", "cnstln1", "sat1");
    manager.setAsDefaultInstance();
    SinkManager::getInstance().registerService("satname");
    REQUIRE(manager.getRegisteredServices().size() == 1);
    REQUIRE(manager.getRegisteredServices().contains({MONITORING, SinkManager::getInstance().getCMDPPort()}));
}

// TODO(stephan.lachnit): test log message decoding

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
