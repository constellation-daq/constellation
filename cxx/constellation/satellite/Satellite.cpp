/**
 * @file
 * @brief Implementation of Satellite class
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Satellite.hpp"

#include <any>
#include <cstdint>
#include <stop_token>
#include <string_view>

#include "constellation/core/logging/log.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/satellite/fsm_definitions.hpp"

using namespace constellation::satellite;
using namespace constellation::utils;

Satellite::Satellite(std::string_view type_name, std::string_view satellite_name)
    : logger_("SATELLITE"), type_name_(type_name), satellite_name_(satellite_name) {}

std::string Satellite::getCanonicalName() const {
    return to_string(type_name_) + "." + to_string(satellite_name_);
}

void Satellite::initializing(const config::Configuration& /* config */) {
    LOG(logger_, INFO) << "Initializing - default";
}

void Satellite::launching() {
    LOG(logger_, INFO) << "Launching - default";
}

void Satellite::landing() {
    LOG(logger_, INFO) << "Landing - default";
}

void Satellite::reconfiguring(const config::Configuration& /* partial_config */) {
    // TODO(stephan.lachnit): throw if not supported
    LOG(logger_, INFO) << "Reconfiguring - default";
}

void Satellite::starting(std::uint32_t run_number) {
    LOG(logger_, INFO) << "Starting run " << run_number << " - default";
}

void Satellite::stopping() {
    LOG(logger_, INFO) << "Stopping - default";
}

void Satellite::running(const std::stop_token& /* stop_token */) {
    LOG(logger_, INFO) << "Running - default";
}

void Satellite::interrupting(State previous_state) {
    LOG(logger_, INFO) << "Interrupting from " << to_string(previous_state) << " - default";
    if(previous_state == State::RUN) {
        LOG(logger_, DEBUG) << "Interrupting: execute stopping";
        stopping();
    }
    LOG(logger_, DEBUG) << "Interrupting: execute landing";
    landing();
}

void Satellite::on_failure(State previous_state) {
    LOG(logger_, INFO) << "Failure from " << to_string(previous_state) << " - default";
}
