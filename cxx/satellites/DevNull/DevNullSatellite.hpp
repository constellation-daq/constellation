/**
 * @file
 * @brief Random data sender satellites
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "constellation/satellite/data/DataReceiver.hpp"
#include "constellation/satellite/Satellite.hpp"

class DevNullSatellite final : public constellation::satellite::Satellite, public constellation::data::DataRecv {
public:
    DevNullSatellite(std::string_view type_name, std::string_view satellite_name);

private:
    void receive(const constellation::message::CDTP1Message& msg) final;
};
