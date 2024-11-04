/**
 * @file
 * @brief Random data transmitting satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stop_token>
#include <string_view>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/satellite/TransmitterSatellite.hpp"

class RandomTransmitterSatellite final : public constellation::satellite::TransmitterSatellite {
public:
    RandomTransmitterSatellite(std::string_view type, std::string_view name);

    void initializing(constellation::config::Configuration& config) final;
    void reconfiguring(const constellation::config::Configuration& partial_config) final;
    void starting(std::string_view run_identifier) final;
    void running(const std::stop_token& stop_token) final;
    void stopping() final;

private:
    static std::uint8_t generate_random_seed();

private:
    std::independent_bits_engine<std::default_random_engine, std::numeric_limits<std::uint8_t>::digits, std::uint8_t>
        byte_rng_;
    std::uint8_t seed_ {};
    std::uint64_t frame_size_ {};
    std::uint32_t number_of_frames_ {};
    std::size_t hwm_reached_ {};
};
