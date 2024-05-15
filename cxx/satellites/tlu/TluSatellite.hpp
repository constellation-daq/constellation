/**
 * @file
 * @brief Caribou Satellite definition
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "constellation/satellite/Satellite.hpp"

using namespace constellation::config;
using namespace constellation::satellite;

class TluSatellite : public Satellite {
public:
    TluSatellite(std::string_view type, std::string_view name);

public:
    void initializing(Configuration& config) override;
    void launching() override;
    void landing() override;
    void reconfiguring(const Configuration& partial_config) override;
    void starting(std::uint32_t run_number) override;
    void stopping() override;
    void running(const std::stop_token& stop_token) override;

private:
    // ToDo: are these needed?
    std::string device_class_;
    Configuration config_;
    std::mutex device_mutex_;

    std::uint64_t frame_nr_;
};
