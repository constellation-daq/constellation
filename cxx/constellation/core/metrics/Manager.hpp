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
#include <thread>

#include "constellation/core/config.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/logging/Logger.hpp"

namespace constellation::metrics {

    /** Manager for Metrics handling & transmission */
    class Manager {
    public:
        using Clock = std::chrono::high_resolution_clock;

    private:
        class Metric {
        public:
            Metric() = default;

            Metric(config::Value value) : value_(value), changed_(true) {}

            virtual void set(const config::Value& value);

            bool check();
            virtual Clock::time_point next_trigger() const { return Clock::time_point::max(); }

        protected:
            virtual bool condition() = 0;

        private:
            config::Value value_ {};
            bool changed_ {false};
        };

        class TimedMetric : public Metric {
        public:
            TimedMetric() = default;

            TimedMetric(Clock::duration interval, config::Value value = {})
                : Metric(value), interval_(interval), last_trigger_(Clock::now()) {}

            bool condition() override;
            Clock::time_point next_trigger() const override;

        private:
            Clock::duration interval_;
            Clock::time_point last_trigger_;
        };

        class TriggeredMetric : public Metric {
        public:
            TriggeredMetric() = default;

            TriggeredMetric(const std::size_t triggers, config::Value value);

            void set(const config::Value& value) override;

            bool condition() override;

        private:
            const std::size_t triggers_;
            std::size_t current_triggers_ {0};
        };

    public:
        /**
         * Return the default CHIRP Manager (requires to be set via `setAsDefaultInstance`)
         *
         * @return Pointer to default CHIRP Manager (might be a nullptr)
         */
        CNSTLN_API static Manager* getDefaultInstance();

        /**
         * Set this CHIRP manager as the default instance
         */
        CNSTLN_API void setAsDefaultInstance();

    public:
        CNSTLN_API Manager() : thread_(std::bind_front(&Manager::run, this)), logger_("STAT") {};

        // No copy/move constructor/assignment
        Manager(Manager& other) = delete;
        Manager& operator=(Manager other) = delete;
        Manager(Manager&& other) = delete;
        Manager& operator=(Manager&& other) = delete;

        CNSTLN_API virtual ~Manager() noexcept;

        /**
         * Update the value cached for the given metric
         *
         * \param topic Unique topic of the metric
         * \param value New value of the metric
         */
        CNSTLN_API void setMetric(const std::string& topic, config::Value value);

        /**
         * Unregister a previously registered metric from the manager
         *
         * @param topic Unique metric topic
         */
        CNSTLN_API void unregisterMetric(std::string topic);

        /**
         * Unregisters all metrics registered in the manager
         *
         * Equivalent to calling `unregisterMetric` for every registered metric.
         */
        CNSTLN_API void unregisterMetrics();

        /**
         * Register a metric which will be emitted after having been triggered a given number of times
         *
         * @param topic Unique topic of the metric
         * @param triggers Minimum number of triggers between consecutive emissions
         * @param value Initial value of the metric
         * @retval true if the metric was registered
         * @retval false if the metric was already registered
         */
        CNSTLN_API bool registerTriggeredMetric(const std::string& topic, std::size_t triggers, config::Value value = {});

        /**
         * Register a metric which will be emitted in regular intervals
         *
         * @param topic Unique topic of the metric
         * @param interval Minimum interval between consecutive emissions
         * @param value Initial value of the metric
         * @retval true if the metric was registered
         * @retval false if the metric was already registered
         */
        CNSTLN_API bool registerTimedMetric(const std::string& topic, Clock::duration interval, config::Value value = {});

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

        /** Map of registered metrics */
        std::map<std::string, std::shared_ptr<Metric>> metrics_;

        /** Main loop thread of the metrics manager */
        std::jthread thread_;

        /** Mutex for thread-safe access to `metrics_` */
        std::mutex mt_;

        /** Conditions variable for waiting until the next metric emission */
        std::condition_variable cv_;

        log::Logger logger_;

        // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
        inline static Manager* default_manager_instance_;
    };

} // namespace constellation::metrics
