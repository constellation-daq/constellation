/**
 * @file
 * @brief BTTB12 satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdint>
#include <string_view>

#include "constellation/satellite/Satellite.hpp"

class bttb12 : public constellation::satellite::Satellite {
public:
    bttb12(std::string_view type_name, std::string_view satellite_name);
    void starting(std::uint32_t run_number) override;
    void running(const std::stop_token& stop_token) override;

private:
    std::vector<std::uint64_t> getDataFromDAQ();

private:
    std::uint64_t frame_number_;
};
