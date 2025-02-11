/**
 * @file
 * @brief CMDPListener implementation
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "LogListener.hpp"

#include <functional>
#include <iomanip>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "constellation/core/log/Level.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/listener/LogListener.hpp"

#include "CMDPListener.hpp"

using namespace constellation::listener;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::utils;

LogListener::LogListener(std::string_view log_topic, std::function<void(CMDP1LogMessage&&)> callback)
    : CMDPListener(log_topic,
                   [callback = std::move(callback)](CMDP1Message&& msg) { callback(CMDP1LogMessage(std::move(msg))); }),
      global_log_level_(Level::OFF) {
    // Subscribe to notifications:
    CMDPListener::subscribeTopic("LOG?");
}

std::vector<std::string> LogListener::generate_topics(const std::string& log_topic, Level level, bool subscribe) {
    std::vector<std::string> topics {};
    const auto lower_level = subscribe ? level : Level::TRACE;
    const auto upper_level = subscribe ? Level::OFF : level;
    for(int level_it = std::to_underlying(lower_level); level_it < std::to_underlying(upper_level); ++level_it) {
        auto topic = "LOG/" + enum_name(Level(level_it));
        if(!log_topic.empty()) [[likely]] {
            topic += "/" + log_topic;
        }
        topics.emplace_back(std::move(topic));
    }
    return topics;
}

std::pair<std::string_view, Level> LogListener::demangle_topic(std::string_view topic) {
    const auto level_endpos = topic.find_first_of('/', 4);
    const auto level_str = topic.substr(4, level_endpos - 4);
    const auto level = (level_str.empty() ? std::optional<Level>(Level::TRACE) : enum_cast<Level>(level_str));
    if(!level.has_value()) {
        std::unreachable();
    }
    const auto log_topic = (level_endpos != std::string_view::npos ? topic.substr(level_endpos + 1) : std::string_view());
    return {log_topic, level.value()};
}

void LogListener::setGlobalLogLevel(Level level) {
    LOG(BasePoolT::pool_logger_, DEBUG) << "Setting global log level to " << level;
    CMDPListener::multiscribeTopics(generate_topics("", level, false), generate_topics("", level));
    global_log_level_.store(level);
}

Level LogListener::getGlobalLogLevel() const {
    return global_log_level_.load();
}

void LogListener::subscribeLogTopic(const std::string& log_topic, log::Level level) {
    // Empty topic not allowed (global level)
    if(log_topic.empty()) [[unlikely]] {
        LOG(BasePoolT::pool_logger_, WARNING) << "Ignoring subscription to empty topic";
        return;
    }
    LOG(BasePoolT::pool_logger_, DEBUG) << "Subscribing to topic " << std::quoted(log_topic) << " with level " << level;
    CMDPListener::multiscribeTopics(generate_topics(log_topic, level, false), generate_topics(log_topic, level));
}

void LogListener::unsubscribeLogTopic(const std::string& log_topic) {
    // Empty topic not allowed (global level)
    if(log_topic.empty()) [[unlikely]] {
        LOG(BasePoolT::pool_logger_, WARNING) << "Ignoring unsubscription from empty topic";
        return;
    }
    LOG(BasePoolT::pool_logger_, DEBUG) << "Unsubscribing from topic " << std::quoted(log_topic);
    CMDPListener::multiscribeTopics(generate_topics(log_topic, Level::TRACE), {});
}

std::map<std::string, Level> LogListener::getLogTopicSubscriptions() {
    std::map<std::string, Level> log_topic_subscriptions {};
    for(const std::string_view topic : CMDPListener::getTopicSubscriptions()) {
        const auto [log_topic, level] = demangle_topic(topic);
        // Do not store global log topic
        if(log_topic.empty()) {
            continue;
        }
        // Check if log topic already stored, if not store, else store min level
        const auto topic_it = log_topic_subscriptions.find(to_string(log_topic));
        if(topic_it == log_topic_subscriptions.end()) {
            log_topic_subscriptions.emplace(log_topic, level);
        } else {
            topic_it->second = min_level(topic_it->second, level);
        }
    }
    return log_topic_subscriptions;
}

void LogListener::subscribeExtaLogTopic(const std::string& host, const std::string& log_topic, log::Level level) {
    LOG(BasePoolT::pool_logger_, DEBUG) << "Subscribing to extra topic " << std::quoted(log_topic) << " with level " << level
                                        << " for host " << host;
    CMDPListener::multiscribeExtraTopics(host, generate_topics(log_topic, level, false), generate_topics(log_topic, level));
}

void LogListener::unsubscribeExtraLogTopic(const std::string& host, const std::string& log_topic) {
    LOG(BasePoolT::pool_logger_, DEBUG) << "Unsubscribing from extra topic " << std::quoted(log_topic) << " for host "
                                        << host;
    CMDPListener::multiscribeExtraTopics(host, generate_topics(log_topic, Level::TRACE), {});
}

std::map<std::string, Level> LogListener::getExtraLogTopicSubscriptions(const std::string& host) {
    std::map<std::string, Level> log_topic_subscriptions {};
    for(const std::string_view topic : CMDPListener::getExtraTopicSubscriptions(host)) {
        const auto [log_topic, level] = demangle_topic(topic);
        // Check if log topic already stored, if not store, else store min level
        const auto topic_it = log_topic_subscriptions.find(to_string(log_topic));
        if(topic_it == log_topic_subscriptions.end()) {
            log_topic_subscriptions.emplace(log_topic, level);
        } else {
            topic_it->second = min_level(topic_it->second, level);
        }
    }
    return log_topic_subscriptions;
}
