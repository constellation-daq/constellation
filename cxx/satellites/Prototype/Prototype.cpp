/**
 * @file
 * @brief Implementation of the prototype satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Prototype.hpp"

#include <string_view>

#include "constellation/core/logging/log.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::satellite;

Prototype::Prototype(std::string_view type, std::string_view name) : Satellite(type, name) {
    LOG(logger_, STATUS) << "Dummy satellite " << getCanonicalName() << " created";
}
