/**
 * @file
 * @brief Implementation of the prototype satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "prototype.hpp"

#include <memory>
#include <string_view>

#include "constellation/core/logging/log.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::satellite;

prototype::prototype(std::string_view name) {
    LOG(logger_, STATUS) << "Dummy satellite " << name << " created";
}

// generator function for loading satellite from shared library
extern "C" std::shared_ptr<Satellite> generator(std::string_view name) {
    return std::make_shared<prototype>(name);
}
