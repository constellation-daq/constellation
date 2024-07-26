/**
 * @file
 * @brief Implementation of the Sputnik prototype satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "SputnikSatellite.hpp"

#include <string_view>

#include "constellation/core/log/log.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::satellite;

SputnikSatellite::SputnikSatellite(std::string_view type, std::string_view name) : Satellite(type, name) {
    LOG(STATUS) << "Sputnik prototype satellite " << getCanonicalName() << " created";
}
