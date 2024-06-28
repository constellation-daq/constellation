/**
 * @file
 * @brief Metrics Manager
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Manager.hpp"

#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <map>
#include <thread>

#include "constellation/core/logging/log.hpp"
#include "constellation/core/logging/SinkManager.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/utils/exceptions.hpp"

using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::metrics;

MetricsManager::~MetricsManager() noexcept {
    thread_.request_stop();
    cv_.notify_all();

    if(thread_.joinable()) {
        thread_.join();
    }
}

void MetricsManager::setMetric(std::string_view topic, const config::Value& value) {

    auto it = metrics_.find(topic);
    if(it != metrics_.end()) {
        it->second->update(value);

        // Notify if this is a triggered metric:
        if(std::dynamic_pointer_cast<TriggeredMetric>(it->second)) {
            cv_.notify_all();
        }
    }
}

void MetricsManager::unregisterMetric(std::string_view topic) {
    const std::lock_guard lock {mt_};
    auto it = metrics_.find(topic);
    if(it != metrics_.end()) {
        metrics_.erase(it);
    }
}

void MetricsManager::registerTriggeredMetric(std::string_view topic,
                                             std::string_view unit,
                                             Type type,
                                             std::size_t triggers,
                                             std::initializer_list<constellation::message::State> states,
                                             const config::Value& value) {
    const std::lock_guard lock {mt_};
    const auto [it, inserted] = metrics_.insert_or_assign(
        std::string(topic), std::make_shared<TriggeredMetric>(unit, type, triggers, states, value));

    if(!inserted) {
        LOG(logger_, INFO) << "Replaced already registered metric " << std::quoted(topic);
    }

    cv_.notify_all();
}

void MetricsManager::registerTimedMetric(std::string_view topic,
                                         std::string_view unit,
                                         Type type,
                                         Clock::duration interval,
                                         std::initializer_list<constellation::message::State> states,
                                         const config::Value& value) {
    const std::lock_guard lock {mt_};
    const auto [it, inserted] =
        metrics_.insert_or_assign(std::string(topic), std::make_shared<TimedMetric>(unit, type, interval, states, value));

    if(!inserted) {
        LOG(logger_, INFO) << "Replaced already registered metric " << std::quoted(topic);
    }

    cv_.notify_all();
}

void MetricsManager::run(const std::stop_token& stop_token) {

    LOG(logger_, TRACE) << "Started metric dispatch thread";

    std::unique_lock<std::mutex> lock {mt_};
    while(!stop_token.stop_requested()) {

        auto next = Clock::time_point::max();
        for(auto& [key, metric] : metrics_) {
            if(metric->check(current_state_)) {
                LOG(logger_, TRACE) << "Timer of metric \"" << key << "\" expired, sending...";

                // Send this metric into the CMDP sink:
                SinkManager::getInstance().sendCMDPMetric(key, metric);
            }

            // Update time point until we can wait:
            next = std::min(next, metric->nextTrigger());
        }

        cv_.wait_until(lock, next, [&]() { return stop_token.stop_requested(); });
    }
}
