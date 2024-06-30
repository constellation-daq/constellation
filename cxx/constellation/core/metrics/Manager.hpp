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
#include <condition_variable>
#include <map>
#include <string_view>
#include <thread>

#include "constellation/build.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/core/message/satellite_definitions.hpp"
#include "constellation/core/metrics/Metric.hpp"

namespace constellation::metrics {

    /** Manager for Metrics handling & transmission */
    class CNSTLN_API MetricsManager {
    public:
        MetricsManager() : logger_("STAT"), thread_(std::bind_front(&MetricsManager::run, this)) {};

        // No copy/move constructor/assignment
        MetricsManager(MetricsManager& other) = delete;
        MetricsManager& operator=(MetricsManager other) = delete;
        MetricsManager(MetricsManager&& other) = delete;
        MetricsManager& operator=(MetricsManager&& other) = delete;

        virtual ~MetricsManager() noexcept;

        /**
         * @brief Update the current state of the sender
         * @details This method will update the state of the FSM such that metrics with designated states are only broadcast
         * when appropriate.
         *
         * @param state New state
         */
        CNSTLN_API void updateState(message::State state) { current_state_ = state; }

        /**
         * Update the value cached for the given metric
         *
         * \param topic Unique topic of the metric
         * \param value New value of the metric
         */
        void setMetric(std::string_view topic, const config::Value& value);

        /**
         * Unregister a previously registered metric from the manager
         *
         * @param topic Unique metric topic
         */
        void unregisterMetric(std::string_view topic);

        /**
         * Unregisters all metrics registered in the manager
         *
         * Equivalent to calling `unregisterMetric` for every registered metric.
         */
        void unregisterMetrics();

        /**
         * Register a metric which will be emitted after having been triggered a given number of times. If the metric exists
         * already, it will be replaced by the new definition.
         *
         * @param topic Unique topic of the metric
         * @param metric_timer Shared pointer to metric timer object
         */
        void registerMetric(std::string_view topic, std::shared_ptr<MetricTimer> metric_timer);

    private:
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

        log::Logger logger_;
        message::State current_state_ {message::State::NEW};

        /** Map of registered metrics */
        std::map<std::string, std::shared_ptr<MetricTimer>, std::less<>> metrics_;

        /** Main loop thread of the metrics manager */
        std::jthread thread_;

        /** Mutex for thread-safe access to `metrics_` */
        std::mutex mt_;

        /** Conditions variable for waiting until the next metric emission */
        std::condition_variable cv_;
    };
} // namespace constellation::metrics
