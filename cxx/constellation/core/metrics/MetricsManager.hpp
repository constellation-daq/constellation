/**
 * @file
 * @brief Metrics Manager
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <concepts>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "constellation/build.hpp"
#include "constellation/core/config/Value.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/metrics/Metric.hpp"
#include "constellation/core/utils/string_hash_map.hpp"
#include "constellation/core/utils/timers.hpp"

namespace constellation::metrics {

    /** Manager for Metrics handling & transmission */
    class MetricsManager {
    public:
        /**
         * @brief Return instance of metrics manager
         */
        CNSTLN_API static MetricsManager& getInstance();

        // No copy/move constructor/assignment
        /// @cond doxygen_suppress
        MetricsManager(MetricsManager& other) = delete;
        MetricsManager& operator=(MetricsManager other) = delete;
        MetricsManager(MetricsManager&& other) = delete;
        MetricsManager& operator=(MetricsManager&& other) = delete;
        /// @endcond

        CNSTLN_API virtual ~MetricsManager() noexcept;

        /**
         * Register a (manually triggered) metric
         *
         * @param metric Shared pointer to the metric
         * @param description Description of the metric
         */
        CNSTLN_API void registerMetric(std::shared_ptr<Metric> metric, std::string description);

        /**
         * Register a (manually triggered) metric
         *
         * @param name Unique topic of the metric
         * @param unit Unit of the provided value
         * @param type Type of the metric
         * @param description Description of the metric
         */
        void registerMetric(std::string name, std::string unit, metrics::MetricType type, std::string description);

        /**
         * Register a timed metric
         *
         * @param metric Shared pointer to the timed metric
         * @param description Description of the metric
         */
        CNSTLN_API void registerTimedMetric(std::shared_ptr<TimedMetric> metric, std::string description);

        /**
         * Register a timed metric
         *
         * @param name Name of the metric
         * @param unit Unit of the metric as human readable string
         * @param type Type of the metric
         * @param description Description of the metric
         * @param interval Interval in which to send the metric
         * @param value_callback Callback to determine the current value of the metric
         */
        template <typename C>
            requires std::invocable<C>
        void registerTimedMetric(std::string name,
                                 std::string unit,
                                 metrics::MetricType type,
                                 std::string description,
                                 std::chrono::steady_clock::duration interval,
                                 C value_callback);

        /**
         * Unregister a previously registered metric from the manager
         *
         * @param name Name of the metric
         */
        CNSTLN_API void unregisterMetric(std::string_view name);

        /**
         * Unregisters all metrics registered in the manager
         *
         * Equivalent to calling `unregisterMetric` for every registered metric.
         */
        CNSTLN_API void unregisterMetrics();

        /**
         * Check if a metric should be send given the subscription status
         */
        CNSTLN_API bool shouldStat(std::string_view name) const;

        /**
         * Manually trigger a metric
         *
         * @param name Name of the metric
         * @param value Value of the metric
         */
        CNSTLN_API void triggerMetric(std::string name, config::Value value);

        /**
         * @brief Update topic subscriptions
         *
         * @param global Global Flag for global subscription to all topics
         * @param topic_subscriptions List of individual subscription topics
         */
        CNSTLN_API void updateSubscriptions(bool global, std::set<std::string_view> topic_subscriptions = {});

    private:
        MetricsManager();

        /**
         * Main loop listening and responding to incoming CHIRP broadcasts
         *
         * The run loop responds to incoming CHIRP broadcasts with REQUEST type by sending CHIRP broadcasts with OFFER type
         * for all registered services. It also tracks incoming CHIRP broadcasts with OFFER and DEPART type to form the list
         * of discovered services and calls the corresponding discovery callbacks.
         *
         * @param stop_token Token to stop loop via `std::jthread`
         */
        void run(const std::stop_token& stop_token);

        class TimedMetricEntry {
        public:
            TimedMetricEntry(std::shared_ptr<TimedMetric> metric)
                : metric_(std::move(metric)), timer_(metric_->interval()) {}

            TimedMetric* operator->() const noexcept { return metric_.get(); }
            std::shared_ptr<Metric> getMetric() { return metric_; }
            bool timeoutReached() const { return timer_.timeoutReached(); }
            void resetTimer() { timer_.reset(); }
            std::chrono::steady_clock::time_point nextTrigger() const { return timer_.startTime() + metric_->interval(); }

        private:
            std::shared_ptr<TimedMetric> metric_;
            utils::TimeoutTimer timer_;
        };

    private:
        log::Logger logger_;

        // List of topics with active subscribers:
        std::set<std::string_view> subscribed_topics_;
        bool global_subscription_;

        // Contains all metrics, including timed ones
        utils::string_hash_map<std::shared_ptr<Metric>> metrics_;
        utils::string_hash_map<std::string> metrics_descriptions_;
        std::mutex metrics_mutex_;

        // Only timed metrics for background thread
        utils::string_hash_map<TimedMetricEntry> timed_metrics_;
        std::mutex timed_metrics_mutex_;

        // Queue for manually triggered metrics
        std::queue<std::pair<std::string, config::Value>> triggered_queue_;
        std::mutex triggered_queue_mutex_;
        std::condition_variable cv_;

        std::jthread thread_;
    };
} // namespace constellation::metrics

// Include template members
#include "MetricsManager.ipp" // IWYU pragma: keep
