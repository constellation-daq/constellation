/**
 * @file
 * @brief Implementation of Satellite class
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Satellite.hpp"

#include <cstdint>
#include <ranges>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/logging/log.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/fsm_definitions.hpp"

using namespace constellation::satellite;
using namespace constellation::utils;

Satellite::Satellite(std::string_view type, std::string_view name)
    : logger_("SATELLITE"), satellite_type_(type), satellite_name_(name) {}

std::string Satellite::getCanonicalName() const {
    return to_string(satellite_type_) + "." + to_string(satellite_name_);
}

void Satellite::initializing(config::Configuration& /* config */) {
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

void Satellite::starting(std::string_view run_identifier) {
    LOG(logger_, INFO) << "Starting run " << run_identifier << " - default";
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

void Satellite::store_config(config::Configuration&& config) {
    using enum config::Configuration::Group;
    using enum config::Configuration::Usage;

    // Check for unused KVPs
    const auto unused_kvps = config.getDictionary(ALL, UNUSED);
    if(!unused_kvps.empty()) {
        LOG(logger_, WARNING) << unused_kvps.size() << " keys of the configuration were not used: "
                              << range_to_string(std::views::keys(unused_kvps));
        // Only store used keys
        config_ = {config.getDictionary(ALL, USED), true};
    } else {
        // Move configuration
        config_ = std::move(config);
    }

    // Log config
    LOG(logger_, INFO) << "Configuration: " << config_.size(USER) << " settings" << config_.getDictionary(USER).to_string();
    LOG(logger_, DEBUG) << "Internal configuration: " << config_.size(INTERNAL) << " settings"
                        << config_.getDictionary(INTERNAL).to_string();
}

void Satellite::update_config(const config::Configuration& partial_config) {
    using enum config::Configuration::Group;
    using enum config::Configuration::Usage;

    // Check for unused KVPs
    const auto unused_kvps = partial_config.getDictionary(ALL, UNUSED);
    if(!unused_kvps.empty()) {
        LOG(logger_, WARNING) << unused_kvps.size() << " keys of the configuration were not used: "
                              << range_to_string(std::views::keys(unused_kvps));
    }

    // Update configuration (only updates used values of partial config)
    config_.update(partial_config);

    // Log config
    LOG(logger_, INFO) << "Configuration: " << config_.size(USER) << " settings" << config_.getDictionary(USER).to_string();
    LOG(logger_, DEBUG) << "Internal configuration: " << config_.size(INTERNAL) << " settings"
                        << config_.getDictionary(INTERNAL).to_string();
}
