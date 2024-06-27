/**
 * @file
 * @brief Implementation of the prototype satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "PrototypeSatellite.hpp"

#include <string_view>

#include "constellation/core/logging/log.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::satellite;

PrototypeSatellite::PrototypeSatellite(std::string_view type, std::string_view name) : Satellite(type, name) {
    LOG(STATUS) << "Dummy satellite " << getCanonicalName() << " created";
}
