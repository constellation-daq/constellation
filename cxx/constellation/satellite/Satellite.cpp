/**
 * @file
 * @brief Implementation of Satellite class
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Satellite.hpp"

#include <stop_token>
#include <string_view>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/BaseSatellite.hpp"

using namespace constellation::metrics;
using namespace constellation::protocol::CSCP;
using namespace constellation::satellite;
using namespace constellation::utils;

Satellite::Satellite(std::string_view type, std::string_view name) : BaseSatellite(type, name) {}

void Satellite::initializing(config::Configuration& /* config */) {}

void Satellite::launching() {}

void Satellite::landing() {}

void Satellite::reconfiguring(const config::Configuration& /* partial_config */) {}

void Satellite::starting(std::string_view /* run_identifier */) {}

void Satellite::stopping() {}

void Satellite::running(const std::stop_token& /* stop_token */) {}

void Satellite::interrupting(State previous_state) {
    LOG(logger_, INFO) << "Interrupting from " << to_string(previous_state) << " (default implementation)";
    if(previous_state == State::RUN) {
        LOG(logger_, DEBUG) << "Interrupting: execute stopping";
        stopping();
    }
    LOG(logger_, DEBUG) << "Interrupting: execute landing";
    landing();
}

void Satellite::failure(State previous_state) {
    LOG(logger_, DEBUG) << "Failure from " << to_string(previous_state) << " (default implementation)";
}


void Satellite::register_timed_metric(std::string_view name,
                                      std::string_view unit,
                                      metrics::Type type,
                                      metrics::Clock::duration interval,
                                      std::initializer_list<State> states,
                                      const config::Value& value) {
    LOG(logger_, DEBUG) << "Registering timed metric \"" << name << "\" to be emitted every "
                        << std::chrono::duration_cast<std::chrono::milliseconds>(interval).count() << "ms";

    metrics_manager_.registerMetric(name, std::make_shared<TimedMetric>(unit, type, interval, states, value));
}

void Satellite::register_timed_metric(std::string_view name,
                                      std::string_view unit,
                                      metrics::Type type,
                                      metrics::Clock::duration interval,
                                      std::initializer_list<State> states,
                                      const std::function<config::Value()>& func) {
    LOG(logger_, DEBUG) << "Registering timed metric \"" << name << "\" to be emitted every "
                        << std::chrono::duration_cast<std::chrono::milliseconds>(interval).count() << "ms";

    metrics_manager_.registerMetric(name, std::make_shared<TimedAutoMetric>(unit, type, interval, states, func));
}

void Satellite::register_triggered_metric(std::string_view name,
                                          std::string_view unit,
                                          metrics::Type type,
                                          std::size_t triggers,
                                          std::initializer_list<State> states,
                                          const config::Value& value) {
    LOG(logger_, DEBUG) << "Registering triggered metric \"" << name << "\" to be emitted every " << triggers << " calls";
    metrics_manager_.registerMetric(name, std::make_shared<TriggeredMetric>(unit, type, triggers, states, value));
}

void Satellite::set_metric(const std::string& topic, const config::Value& value) {
    metrics_manager_.setMetric(topic, value);
}
