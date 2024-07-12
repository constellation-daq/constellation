/**
 * @file
 * @brief Satellite class with transitional user functions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <functional>
#include <initializer_list>
#include <memory>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>

#include "constellation/build.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/metrics/Metric.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/satellite/BaseSatellite.hpp"
#include "constellation/satellite/CommandRegistry.hpp"

namespace constellation::satellite {

    /**
     * @brief Satellite class with transitional user functions
     */
    class CNSTLN_API Satellite : public BaseSatellite {
    public:
        virtual ~Satellite() = default;

        // No copy/move constructor/assignment
        /// @cond doxygen_suppress
        Satellite(const Satellite& other) = delete;
        Satellite& operator=(const Satellite& other) = delete;
        Satellite(Satellite&& other) = delete;
        Satellite& operator=(Satellite&& other) = delete;
        /// @endcond

    public:
        /**
         * @brief Initialize satellite
         *
         * In this function a satellite can for example check the configuration or establish connection to a device.
         *
         * @note A satellite can be re-initializied from INIT, i.e. this function can be called twice in a row. Any actions
         *       required to be undone before another initialization should be done in `launching()` instead.
         *
         * @param config Configuration of the satellite
         */
        void initializing(config::Configuration& config) override;

        /**
         * @brief Launch satellite
         *
         * In this function the configuration should be applied and the satellite prepared for data taking, for example by
         * ramping up the high voltage of a device.
         */
        void launching() override;

        /**
         * @brief Land satellite
         *
         * In this function should actions performed in the `launching()` function should be undone, for example by ramping
         * down the high voltage of a device.
         */
        void landing() override;

        /**
         * @brief Reconfigure satellite
         *
         * In this function a partial configuration should be applied to the already launched satellite. This function should
         * throw if a configuration parameter is changed that is not supported in online reconfiguration.
         *
         * @note By default, the satellite does not support online reconfiguration. Support for online reconfiguration can be
         *       enabled with `support_reconfigure()`.
         *
         * @param partial_config Changes to the configuration of the satellite
         */
        void reconfiguring(const config::Configuration& partial_config) override;

        /**
         * @brief Start satellite
         *
         * In this function the data acquisition of the satellite should be started, for example by opening the output file.
         *
         * @note This function should not take a long time to execute. Slow actions such as applying a configuration should
         *       be performed in the `launching()` function.
         *
         * @param run_identifier Run identifier for the upcoming run
         */
        void starting(std::string_view run_identifier) override;

        /**
         * @brief Stop satellite
         *
         * In this function the data acquisition of the satellite should be stopped, for example by closing the output file.
         */
        void stopping() override;

        /**
         * @brief Run function
         *
         * In this function the data acquisition of the satellite should be continuously executed.
         *
         * @param stop_token Token which tracks if running should be stopped or aborted
         */
        void running(const std::stop_token& stop_token) override;

        /**
         * @brief Interrupt function
         *
         * In this function a response for the transition from ORBIT or RUN to the SAFE state can be implemented. This
         * includes for example closing open files or turning off the high voltage. By default, this function calls
         * `stopping()` (if in RUN state) and then `landing()`.
         *
         * @param previous_state State in which the satellite was being interrupted
         */
        void interrupting(protocol::CSCP::State previous_state) override;

        /**
         * @brief Failure function
         *
         * In this function a response to uncatched errors can be implemented. It is executed after entering the ERROR state.
         *
         * @param previous_state State in which the satellite was before experiencing a failure
         */
        void failure(protocol::CSCP::State previous_state) override;

    protected:
        /**
         * @brief Construct a satellite
         *
         * @param type Satellite type
         * @param name Name of this satellite instance
         */
        Satellite(std::string_view type, std::string_view name);

        /**
         * @brief Enable or disable support for reconfigure transition
         *
         * Required to enable the `reconfiguring()` function (disabled by default).
         *
         * @param enable If online reconfiguration support should be enabled
         */
        constexpr void support_reconfigure(bool enable = true) { support_reconfigure_ = enable; }

        /**
         * @brief Register a metric which will be emitted in regular intervals
         *
         * @param name Unique topic of the metric
         * @param unit Unit of the provided value
         * @param type Type of the metric
         * @param interval Minimum interval between consecutive emissions
         * @param states States in which this metric will be emitted, empty list will always broadcast this metric
         * @param value Initial value of the metric
         */
        void register_timed_metric(std::string_view name,
                                   std::string_view unit,
                                   metrics::Type type,
                                   metrics::Clock::duration interval,
                                   std::initializer_list<State> states = {},
                                   const config::Value& value = {}) {
            metrics_manager_.registerMetric(name,
                                            std::make_shared<metrics::TimedMetric>(unit, type, interval, states, value));
        }

        /**
         * @brief Register a metric which will be emitted in regular intervals, evaluated from the provided function
         *
         * @param name Unique topic of the metric
         * @param unit Unit of the provided value
         * @param type Type of the metric
         * @param interval Minimum interval between consecutive emissions
         * @param states States in which this metric will be emitted, empty list will always broadcast this metric
         * @param func Function to evaluate for this metric
         */
        void register_timed_metric(std::string_view name,
                                   std::string_view unit,
                                   metrics::Type type,
                                   metrics::Clock::duration interval,
                                   std::initializer_list<State> states,
                                   const std::function<config::Value()>& func) {
            metrics_manager_.registerMetric(name,
                                            std::make_shared<metrics::TimedAutoMetric>(unit, type, interval, states, func));
        }

        /**
         * @brief Register a metric which will be emitted after having been triggered a given number of times
         *
         * @param name Unique topic of the metric
         * @param unit Unit of the provided value
         * @param type Type of the metric
         * @param triggers Minimum number of triggers between consecutive emissions
         * @param states States in which this metric will be emitted, empty list will always broadcast this metric
         * @param value Initial value of the metric
         */
        void register_triggered_metric(std::string_view name,
                                       std::string_view unit,
                                       metrics::Type type,
                                       std::size_t triggers,
                                       std::initializer_list<State> states = {},
                                       const config::Value& value = {}) {
            metrics_manager_.registerMetric(name,
                                            std::make_shared<metrics::TriggeredMetric>(unit, type, triggers, states, value));
        }

        /**
         * @brief Update the value cached for the given metric
         *
         * @param topic Unique topic of the metric
         * @param value New value of the metric
         */
        void set_metric(const std::string& topic, const config::Value& value) { metrics_manager_.setMetric(topic, value); }

        /**
         * @brief Register a new user command
         *
         * @param name Name of the command
         * @param description Comprehensive description of the command
         * @param states States of the finite state machine in which this command can be called
         * @param func Pointer to the member function to be called
         * @param t Pointer to the satellite object
         */
        template <typename T, typename R, typename... Args>
        void register_command(const std::string& name,
                              std::string description,
                              std::initializer_list<protocol::CSCP::State> states,
                              R (T::*func)(Args...),
                              T* t) {
            user_commands_.add(name, std::move(description), states, func, t);
        }

        /**
         * @brief Register a new user command from a function or lambda
         *
         * @param name Name of the command
         * @param description Comprehensive description of the command
         * @param states States of the finite state machine in which this command can be called
         * @param func Function to be called
         */
        template <typename R, typename... Args>
        void register_command(const std::string& name,
                              std::string description,
                              std::initializer_list<protocol::CSCP::State> states,
                              std::function<R(Args...)> func) {
            user_commands_.add(name, std::move(description), states, func);
        }
    };

    // Generator function that needs to be exported in a satellite library
    using Generator = std::shared_ptr<Satellite>(std::string_view, std::string_view);

} // namespace constellation::satellite
