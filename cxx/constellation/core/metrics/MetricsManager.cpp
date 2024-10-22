/**
 * @file
 * @brief Metrics Manager
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "MetricsManager.hpp"

#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <map>
#include <thread>

#include "constellation/core/log/log.hpp"
#include "constellation/core/log/SinkManager.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/utils/exceptions.hpp"

using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::metrics;
using namespace constellation::protocol;

MetricsManager::MetricsManager(std::function<CSCP::State()> state_callback)
    : logger_("STAT"), state_callback_(std::move(state_callback)), thread_(std::bind_front(&MetricsManager::run, this)) {};

MetricsManager::~MetricsManager() noexcept {
    thread_.request_stop();
    cv_.notify_all();

    if(thread_.joinable()) {
        thread_.join();
    }
}

void MetricsManager::setMetric(std::string_view topic, config::Value&& value) {
    const std::lock_guard lock {mt_};
    auto it = metrics_.find(topic);
    if(it != metrics_.end()) {
        it->second->update(std::move(value));

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

void MetricsManager::registerMetric(std::string_view topic, std::shared_ptr<MetricTimer> metric_timer) {
    const std::lock_guard lock {mt_};
    const auto [it, inserted] = metrics_.insert_or_assign(std::string(topic), metric_timer);

    if(!inserted) {
        LOG(logger_, INFO) << "Replaced already registered metric " << std::quoted(topic);
    }

    LOG(logger_, DEBUG) << "Successfully registered metric " << std::quoted(topic);
    cv_.notify_all();
}

void MetricsManager::run(const std::stop_token& stop_token) {

    LOG(logger_, TRACE) << "Started metric dispatch thread";

    std::unique_lock<std::mutex> lock {mt_};
    while(!stop_token.stop_requested()) {

        auto next = std::chrono::high_resolution_clock::time_point::max();
        for(auto& [key, metric] : metrics_) {
            if(metric->check(state_callback_())) {
                LOG(logger_, TRACE) << "Timer of metric \"" << key << "\" expired, sending...";

                // Send this metric into the CMDP sink:
                SinkManager::getInstance().sendCMDPMetric(key, metric);

                // If we tell the receiving side to accumulate, we need to reset locally:
                if(metric->type() == Type::ACCUMULATE) {
                    metric->set({});
                }
            }

            // Update time point until we can wait:
            next = std::min(next, metric->nextTrigger());
        }

        // Wait until notified or timed out:
        cv_.wait_until(lock, next);
        LOG(logger_, TRACE) << "Metrics condition timed out or got notified";
    }
}
