/**
 * @file
 * @brief Implementation of TLU Satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "TluSatellite.hpp"

#include <unistd.h>

#include "constellation/core/logging/log.hpp"
#include "constellation/satellite/Satellite.hpp"

// generator function for loading satellite from shared library
extern "C" std::shared_ptr<Satellite> generator(std::string_view type_name, std::string_view satellite_name) {
    return std::make_shared<TluSatellite>(type_name, satellite_name);
}

TluSatellite::TluSatellite(std::string_view type, std::string_view name) : Satellite(type, name) {
    LOG(logger_, STATUS) << "TluSatellite " << getCanonicalName() << " created";
}

void TluSatellite::initializing(constellation::config::Configuration& config) {
    LOG(logger_, INFO) << "Initializing " << getCanonicalName();

    // Dummy use of configuration
    config.setDefault("banana", 1337);
}

void TluSatellite::launching() {
    LOG(logger_, INFO) << "Launching TLU";
}

void TluSatellite::landing() {
    LOG(logger_, INFO) << "Landing TLU";
}

void TluSatellite::reconfiguring(const constellation::config::Configuration& /*partial_config*/) {}

void TluSatellite::starting(std::uint32_t run_number) {
    LOG(logger_, INFO) << "Starting run " << run_number << "...";
}

void TluSatellite::stopping() {
    LOG(logger_, INFO) << "Stopping run...";
}

void TluSatellite::running(const std::stop_token& stop_token) {
    LOG(logger_, INFO) << "Starting run loop...";
    sleep(1);
    LOG(logger_, INFO) << "Running, ";

    while(!stop_token.stop_requested()) {
        // sleep 1s
        sleep(1);
        LOG(logger_, INFO) << "...keep on running";
    }

    LOG(logger_, INFO) << "Exiting run loop";
}
