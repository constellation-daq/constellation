/**
 * @file
 * @brief Implementation of random data transmitting satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "SPIDRSatellite.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <random>
#include <stop_token>
#include <string_view>
#include <utility>
#include <vector>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/TransmitterSatellite.hpp"

using namespace constellation::config;
using namespace constellation::satellite;
using namespace constellation::utils;

SPIDRSatellite::SPIDRSatellite(std::string_view type, std::string_view name) : TransmitterSatellite(type, name) {}

void SPIDRSatellite::initializing(Configuration& config) {}

void SPIDRSatellite::starting(std::string_view run_identifier) {}

void SPIDRSatellite::running(const std::stop_token& stop_token) {
    while(!stop_token.stop_requested()) {
    }
}

void SPIDRSatellite::stopping() {}
