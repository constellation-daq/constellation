/**
 * @file
 * @brief Implementation of Logger
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Logger.hpp"

#include <chrono> // IWYU pragma: keep
#include <string_view>
#include <thread>

#include "constellation/core/log/SinkManager.hpp"

using namespace constellation::log;
using namespace std::chrono_literals;

Logger::Logger(std::string_view topic) : spdlog_logger_(SinkManager::getInstance().getLogger(topic)) {}

Logger& Logger::getDefault() {
    static Logger instance {SinkManager::getInstance().getDefaultLogger()};
    return instance;
}

Logger::~Logger() {
    flush();
}

void Logger::flush() {
    for(auto& sink : spdlog_logger_->sinks()) {
        sink->flush();
    }
    // Wait a bit to ensure console output is actually flushed
    std::this_thread::sleep_for(1ms);
}
