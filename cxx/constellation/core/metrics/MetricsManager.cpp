/**
 * @file
 * @brief Metrics Manager
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "MetricsManager.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iomanip>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

#include "constellation/core/config/Value.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/log/SinkManager.hpp"
#include "constellation/core/metrics/Metric.hpp"

using namespace constellation::config;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::metrics;
using namespace std::chrono_literals;

MetricsManager& MetricsManager::getInstance() {
    static MetricsManager instance {};
    return instance;
}

MetricsManager::MetricsManager() : logger_("STAT"), thread_(std::bind_front(&MetricsManager::run, this)) {};

MetricsManager::~MetricsManager() noexcept {
    thread_.request_stop();
    cv_.notify_one();

    if(thread_.joinable()) {
        thread_.join();
    }
}

bool MetricsManager::shouldStat(std::string_view name) const {
    return global_subscription_ || subscribed_topics_.contains(name);
}

void MetricsManager::updateSubscriptions(bool global, std::set<std::string_view> topic_subscriptions) {
    // Acquire lock for metric variables and update them
    const std::lock_guard levels_lock {metrics_mutex_};
    global_subscription_ = global;
    subscribed_topics_ = std::move(topic_subscriptions);
}

void MetricsManager::registerMetric(std::shared_ptr<Metric> metric) {
    const auto name = std::string(metric->name());

    std::unique_lock metrics_lock {metrics_mutex_};
    const auto [it, inserted] = metrics_.insert_or_assign(name, std::move(metric));
    send_notification();
    metrics_lock.unlock();

    if(!inserted) {
        // Erase from timed metrics map in case previously registered as timed metric
        std::unique_lock timed_metrics_lock {timed_metrics_mutex_};
        timed_metrics_.erase(name);
        timed_metrics_lock.unlock();
        LOG(logger_, DEBUG) << "Replaced already registered metric " << std::quoted(name);
    }

    LOG(logger_, DEBUG) << "Successfully registered metric " << std::quoted(name);
}

void MetricsManager::registerTimedMetric(std::shared_ptr<TimedMetric> metric) {
    const auto name = std::string(metric->name());

    // Add to metrics map
    std::unique_lock metrics_lock {metrics_mutex_};
    const auto [it, inserted] = metrics_.insert_or_assign(name, metric);
    send_notification();
    metrics_lock.unlock();

    if(!inserted) {
        LOG(logger_, DEBUG) << "Replaced already registered metric " << std::quoted(name);
    }

    // Now also add to timed metrics map
    std::unique_lock timed_metrics_lock {timed_metrics_mutex_};
    timed_metrics_.insert_or_assign(name, TimedMetricEntry(std::move(metric)));
    timed_metrics_lock.unlock();

    LOG(logger_, DEBUG) << "Successfully registered timed metric " << std::quoted(name);

    // Trigger loop to send timed metric immediately
    cv_.notify_one();
}

void MetricsManager::unregisterMetric(std::string_view name) {
    std::unique_lock metrics_lock {metrics_mutex_};
    auto it = metrics_.find(name);
    if(it != metrics_.end()) {
        metrics_.erase(it);
    }
    send_notification();
    metrics_lock.unlock();

    std::unique_lock timed_metrics_lock {timed_metrics_mutex_};
    auto it_timed = timed_metrics_.find(name);
    if(it_timed != timed_metrics_.end()) {
        timed_metrics_.erase(it_timed);
    }
    timed_metrics_lock.unlock();
}

void MetricsManager::unregisterMetrics() {
    std::unique_lock metrics_lock {metrics_mutex_};
    metrics_.clear();
    send_notification();
    metrics_lock.unlock();

    std::unique_lock timed_metrics_lock {timed_metrics_mutex_};
    timed_metrics_.clear();
    timed_metrics_lock.unlock();
}

void MetricsManager::send_notification() const {

    Dictionary metrics;
    for(const auto& [name, metric]: metrics_) {
        metrics.emplace(name, std::string(metric->name()));
    }
    SinkManager::getInstance().sendMetricNotification(std::move(metrics));
}

void MetricsManager::triggerMetric(std::string name, Value value) {
    // Only emplace name and value, do the lookup in the run thread
    std::unique_lock triggered_queue_lock {triggered_queue_mutex_};
    triggered_queue_.emplace(std::move(name), std::move(value));
    triggered_queue_lock.unlock();
    cv_.notify_one();
}

void MetricsManager::run(const std::stop_token& stop_token) {
    LOG(logger_, TRACE) << "Started metric dispatch thread";

    // Notify condition variable when stop is requested
    const std::stop_callback stop_callback {stop_token, [&]() { cv_.notify_one(); }};

    auto wakeup = std::chrono::steady_clock::now() + 1s;

    while(!stop_token.stop_requested()) {
        std::unique_lock triggered_queue_lock {triggered_queue_mutex_};
        // Wait until condition variable is notified or timeout is reached
        cv_.wait_until(triggered_queue_lock, wakeup);

        // Send any triggered metrics in the queue
        while(!triggered_queue_.empty()) {
            auto [name, value] = std::move(triggered_queue_.front());
            triggered_queue_.pop();
            LOG(logger_, TRACE) << "Looking for queued metric " << std::quoted(name);
            const std::lock_guard metrics_lock {metrics_mutex_};
            auto metric_it = metrics_.find(name);
            if(metric_it != metrics_.end()) {
                LOG(logger_, TRACE) << "Sending metric " << std::quoted(name) << ": " << value.str() << " ["
                                    << metric_it->second->unit() << "]";
                SinkManager::getInstance().sendCMDPMetric({metric_it->second, std::move(value)});
            } else {
                LOG(logger_, WARNING) << "Metric " << std::quoted(name) << " is not registered";
            }
        }
        triggered_queue_lock.unlock();

        // Set next wakeup to 1s from now
        const auto now = std::chrono::steady_clock::now();
        wakeup = now + 1s;

        // Check timed metrics
        const std::lock_guard timed_metrics_lock {timed_metrics_mutex_};
        for(auto& [name, timed_metric] : timed_metrics_) {
            // If last time sent larger than interval and allowed and there is a subscription -> send metric
            if(timed_metric.timeoutReached() && shouldStat(name)) {
                auto value = timed_metric->currentValue();
                if(value.has_value()) {
                    LOG(logger_, TRACE) << "Sending metric " << std::quoted(timed_metric->name()) << ": "
                                        << value.value().str() << " [" << timed_metric->unit() << "]";
                    SinkManager::getInstance().sendCMDPMetric({timed_metric.getMetric(), std::move(value.value())});
                    timed_metric.resetTimer();
                } else {
                    LOG(logger_, TRACE) << "Not sending metric " << std::quoted(timed_metric->name()) << ": no value";
                }
            }
            // Update time point until we have to wait (if not in the past)
            const auto next_trigger = timed_metric.nextTrigger();
            if(next_trigger - now > std::chrono::steady_clock::duration::zero()) {
                wakeup = std::min(wakeup, next_trigger);
            }
        }
    }
}
