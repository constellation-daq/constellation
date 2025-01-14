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
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "constellation/core/log/Level.hpp"
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
      global_log_level_(Level::OFF) {}

std::vector<std::string> LogListener::generate_topics(const std::string& log_topic, Level level) {
    std::vector<std::string> topics {};
    for(int level_it = std::to_underlying(level); level_it < std::to_underlying(Level::OFF); ++level_it) {
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
    for(int level_it = std::to_underlying(Level::TRACE); level_it < std::to_underlying(Level::OFF); ++level_it) {
        // If level is lower than new level unsubscribe, otherwise subscribe
        // We do not need to worry about duplicate (un-)subscription thanks to LogListener logik
        if(level_it < std::to_underlying(level)) {
            CMDPListener::unsubscribeTopic("LOG/" + enum_name(Level(level_it)));
        } else {
            CMDPListener::subscribeTopic("LOG/" + enum_name(Level(level_it)));
        }
    }
    global_log_level_.store(level);
}

Level LogListener::getGlobalLogLevel() const {
    return global_log_level_.load();
}

void LogListener::subscribeLogTopic(const std::string& log_topic, log::Level level) {
    for(auto&& gen_topic : generate_topics(log_topic, level)) {
        CMDPListener::subscribeTopic(std::move(gen_topic));
    }
}

void LogListener::unsubscribeLogTopic(const std::string& log_topic) {
    for(const auto& gen_topic : generate_topics(log_topic, Level::TRACE)) {
        CMDPListener::unsubscribeTopic(gen_topic);
    }
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
    for(auto&& gen_topic : generate_topics(log_topic, level)) {
        CMDPListener::subscribeExtraTopic(host, std::move(gen_topic));
    }
}

void LogListener::unsubscribeExtraLogTopic(const std::string& host, const std::string& log_topic) {
    for(const auto& gen_topic : generate_topics(log_topic, Level::TRACE)) {
        CMDPListener::unsubscribeExtraTopic(host, gen_topic);
    }
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
