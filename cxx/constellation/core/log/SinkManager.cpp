/**
 * @file
 * @brief Implementation of the Log Sink Manager
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "SinkManager.hpp"

#include <ctime>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/common.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <wincon.h>
#endif

#include "constellation/core/log/CMDPSink.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/log/ProxySink.hpp"
#include "constellation/core/utils/networking.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::log;
using namespace constellation::utils;

SinkManager::ConstellationLevelFormatter::ConstellationLevelFormatter(bool format_short) : format_short_(format_short) {}

void SinkManager::ConstellationLevelFormatter::format(const spdlog::details::log_msg& msg,
                                                      const std::tm& /*tm*/,
                                                      spdlog::memory_buf_t& dest) {
    auto level_name = to_string(from_spdlog_level(msg.level));
    if(format_short_) {
        // Short format: only first letter
        level_name = level_name.substr(0, 1);
    } else {
        // Long format: pad to 8 characters
        level_name.insert(0, 8 - level_name.size(), ' ');
    }
    dest.append(level_name.data(), level_name.data() + level_name.size());
}

std::unique_ptr<spdlog::custom_flag_formatter> SinkManager::ConstellationLevelFormatter::clone() const {
    return std::make_unique<ConstellationLevelFormatter>(format_short_);
}

void SinkManager::ConstellationTopicFormatter::format(const spdlog::details::log_msg& msg,
                                                      const std::tm& /*tm*/,
                                                      spdlog::memory_buf_t& dest) {
    if(msg.logger_name.size() > 0) {                         // NOLINT(readability-container-size-empty) might be fmt string
        auto topic = "[" + to_string(msg.logger_name) + "]"; // NOLINT(misc-include-cleaner) might be fmt string
        dest.append(topic.data(), topic.data() + topic.size());
    }
}

std::unique_ptr<spdlog::custom_flag_formatter> SinkManager::ConstellationTopicFormatter::clone() const {
    return std::make_unique<ConstellationTopicFormatter>();
}

SinkManager& SinkManager::getInstance() {
    static SinkManager instance {};
    return instance;
}

SinkManager::SinkManager() : zmq_context_(global_zmq_context()), console_global_level_(TRACE), cmdp_global_level_(OFF) {
    // Init thread pool with 1k queue size on 1 thread
    spdlog::init_thread_pool(1000, 1);

    // Concole sink, log level always TRACE since only accessed via ProxySink
    console_sink_ = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink_->set_level(to_spdlog_level(TRACE));

    // Set console format, e.g. |2024-01-10 00:16:40.922| CRITICAL [topic] message
    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<ConstellationLevelFormatter>('l', false);
    formatter->add_flag<ConstellationLevelFormatter>('L', true);
    formatter->add_flag<ConstellationTopicFormatter>('n');
    formatter->set_pattern("|%Y-%m-%d %H:%M:%S.%e| %^%l%$ %n %v");
    console_sink_->set_formatter(std::move(formatter));

    // Set colors of console sink
#ifdef _WIN32
    console_sink_->set_color(to_spdlog_level(CRITICAL), FOREGROUND_RED | FOREGROUND_INTENSITY);
    console_sink_->set_color(to_spdlog_level(STATUS), FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    console_sink_->set_color(to_spdlog_level(WARNING), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    console_sink_->set_color(to_spdlog_level(INFO), FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
    console_sink_->set_color(to_spdlog_level(DEBUG), FOREGROUND_GREEN | FOREGROUND_BLUE);
    console_sink_->set_color(to_spdlog_level(TRACE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
    console_sink_->set_color(to_spdlog_level(CRITICAL), "\x1B[31;1m"); // Bold red
    console_sink_->set_color(to_spdlog_level(STATUS), "\x1B[32;1m");   // Bold green
    console_sink_->set_color(to_spdlog_level(WARNING), "\x1B[33;1m");  // Bold yellow
    console_sink_->set_color(to_spdlog_level(INFO), "\x1B[36;1m");     // Bold cyan
    console_sink_->set_color(to_spdlog_level(DEBUG), "\x1B[36m");      // Cyan
    console_sink_->set_color(to_spdlog_level(TRACE), "\x1B[90m");      // Grey
#endif

    // CMDP sink, log level always TRACE since only accessed via ProxySink
    cmdp_sink_ = std::make_shared<CMDPSink>(zmq_context_);
    cmdp_sink_->set_level(to_spdlog_level(TRACE));

    // Create default logger without topic
    default_logger_ = create_logger("DEFAULT");
}

void SinkManager::enableCMDPSending(std::string sender_name) {
    cmdp_sink_->enableSending(std::move(sender_name));
}

std::shared_ptr<spdlog::async_logger> SinkManager::getLogger(std::string_view topic) {
    // Acquire lock for loggers_
    std::unique_lock loggers_lock {loggers_mutex_};
    // Check if logger with topic already exists and if so return
    for(const auto& logger : loggers_) {
        if(logger->name() == topic) {
            return logger;
        }
    }
    // If not found unlock lock and create new logger
    loggers_lock.unlock();
    return create_logger(to_string(topic));
}

std::shared_ptr<spdlog::async_logger> SinkManager::create_logger(std::string_view topic) {
    // Create proxy for CMDP sink so that we can set CMDP log level separate from console log level
    auto cmdp_proxy_sink = std::make_shared<ProxySink>(cmdp_sink_);

    // Create proxy for console sink
    auto console_proxy_sink = std::make_shared<ProxySink>(console_sink_);

    // Create logger with upper-case topic
    auto logger = std::make_shared<spdlog::async_logger>(
        transform(topic, ::toupper),
        spdlog::sinks_init_list({std::move(console_proxy_sink), std::move(cmdp_proxy_sink)}),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::overrun_oldest);

    // Acquire lock for loggers_ and add to new logger
    std::unique_lock loggers_lock {loggers_mutex_};
    loggers_.push_back(logger);
    loggers_lock.unlock();

    // Calculate level of logger and its sinks
    calculate_log_level(logger);

    return logger;
}

void SinkManager::calculate_log_level(std::shared_ptr<spdlog::async_logger>& logger) {
    // Order of sinks given in create_logger()
    auto& console_proxy_sink = logger->sinks().at(0);
    auto& cmdp_proxy_sink = logger->sinks().at(1);

    // Acquire lock for level variables
    std::unique_lock levels_lock {levels_mutex_};

    // For console first set proxy level to global level
    Level new_console_level = console_global_level_;

    // Iterate over topic overwrites
    for(const auto& [console_topic, console_level] : console_topic_levels_) {
        if(logger->name() == console_topic) {
            new_console_level = console_level;
            break;
        }
    }

    // For CMDP first set proxy level to global CMDP minimum
    Level min_cmdp_proxy_level = cmdp_global_level_;

    // Ignore logger without topic as it cannot be unsubscribed
    if(!logger->name().empty()) {
        // Iterate over topic subscriptions to find minimum level for this logger
        for(auto& [sub_topic, sub_level] : cmdp_sub_topic_levels_) {
            if(logger->name().starts_with(sub_topic)) {
                // Logger is subscribed => set new minimum level
                min_cmdp_proxy_level = min_level(min_cmdp_proxy_level, sub_level);
            }
        }
    }

    levels_lock.unlock();

    // Set new levels for console and CMDP proxy
    console_proxy_sink->set_level(to_spdlog_level(new_console_level));
    cmdp_proxy_sink->set_level(to_spdlog_level(min_cmdp_proxy_level));

    // Calculate level for logger as minimum of both proxy levels
    logger->set_level(to_spdlog_level(min_level(new_console_level, min_cmdp_proxy_level)));
}

void SinkManager::setConsoleLevels(Level global_level, std::map<std::string, Level> topic_levels) {
    // Acquire lock for level variables and update them
    std::unique_lock levels_lock {levels_mutex_};
    console_global_level_ = global_level;
    console_topic_levels_ = std::move(topic_levels);
    levels_lock.unlock();

    // Acquire lock to prevent modification of loggers_
    const std::lock_guard loggers_lock {loggers_mutex_};
    // Set re-calculate log level for every logger
    for(auto& logger : loggers_) {
        calculate_log_level(logger);
    }
}

void SinkManager::updateCMDPLevels(Level cmdp_global_level, std::map<std::string_view, Level> cmdp_sub_topic_levels) {
    // Acquire lock for level variables and update them
    std::unique_lock levels_lock {levels_mutex_};
    cmdp_global_level_ = cmdp_global_level;
    cmdp_sub_topic_levels_ = std::move(cmdp_sub_topic_levels);
    levels_lock.unlock();

    // Acquire lock for loggers_
    const std::lock_guard loggers_lock {loggers_mutex_};
    // Set re-calculate log level for every logger
    for(auto& logger : loggers_) {
        calculate_log_level(logger);
    }
}
