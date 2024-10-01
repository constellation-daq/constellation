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
