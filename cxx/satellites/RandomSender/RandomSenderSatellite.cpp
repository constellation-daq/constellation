/**
 * @file
 * @brief Implementation of random data sender satellites
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "RandomSenderSatellite.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <random>
#include <stop_token>
#include <string_view>
#include <utility>
#include <vector>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/logging/log.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::config;
using namespace constellation::satellite;
using namespace constellation::utils;

RandomSenderSatellite::RandomSenderSatellite(std::string_view type_name, std::string_view satellite_name)
    : Satellite(type_name, satellite_name), data_sender_(getCanonicalName()), byte_rng_(generate_random_seed()) {}

std::uint8_t RandomSenderSatellite::generate_random_seed() {
    std::random_device rng {};
    return static_cast<std::uint8_t>(rng());
}

void RandomSenderSatellite::initializing(Configuration& config) {
    seed_ = config.get<std::uint8_t>("seed", generate_random_seed());
    frame_size_ = config.get<std::uint64_t>("frame_size", 1024U);
    number_of_frames_ = config.get<std::uint32_t>("number_of_frames", 1U);
    LOG(STATUS) << "Initialized with seed " << to_string(seed_) << " and " << frame_size_
                << " bytes per data frame, sending " << number_of_frames_ << " "
                << (number_of_frames_ == 1 ? "frame" : "frames") << " per message";
    data_sender_.initializing(config);
}

void RandomSenderSatellite::starting(std::string_view run_identifier) {
    byte_rng_.seed(seed_);
    data_sender_.starting(getConfig());
    LOG(INFO) << "Starting run " << run_identifier << " with seed " << to_string(seed_);
}

void RandomSenderSatellite::running(const std::stop_token& stop_token) {
    while(!stop_token.stop_requested()) {
        data_sender_.newDataMessage(number_of_frames_);
        for(std::uint32_t n = 0; n < number_of_frames_; ++n) {
            // Generate random bytes
            std::vector<std::uint8_t> data {};
            data.resize(frame_size_);
            std::generate(data.begin(), data.end(), std::ref(byte_rng_));
            // Send data
            data_sender_.addDataToMessage(std::move(data));
        }
        data_sender_.sendDataMessage();
    }
}

void RandomSenderSatellite::stopping() {
    data_sender_.stopping();
}
