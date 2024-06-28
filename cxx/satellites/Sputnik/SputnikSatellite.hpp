/**
 * @file
 * @brief Prototype satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <string_view>

#include "constellation/satellite/Satellite.hpp"

class SputnikSatellite final : public constellation::satellite::Satellite {
public:
    SputnikSatellite(std::string_view type, std::string_view name);
};
