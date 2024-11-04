/**
 * @file
 * @brief Implementation of random data transmitting satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "RandomTransmitterSatellite.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <random>
#include <stop_token>
#include <string_view>
#include <utility>
#include <vector>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/TransmitterSatellite.hpp"

using namespace constellation::config;
using namespace constellation::satellite;
using namespace constellation::utils;

RandomTransmitterSatellite::RandomTransmitterSatellite(std::string_view type, std::string_view name)
    : TransmitterSatellite(type, name), byte_rng_(generate_random_seed()) {
    support_reconfigure();
}

std::uint8_t RandomTransmitterSatellite::generate_random_seed() {
    std::random_device rng {};
    return static_cast<std::uint8_t>(rng());
}

void RandomTransmitterSatellite::initializing(Configuration& config) {
    seed_ = config.get<std::uint8_t>("seed", generate_random_seed());
    frame_size_ = config.get<std::uint64_t>("frame_size", 1024U);
    number_of_frames_ = config.get<std::uint32_t>("number_of_frames", 1U);
    LOG(STATUS) << "Initialized with seed " << to_string(seed_) << " and " << frame_size_
                << " bytes per data frame, sending " << number_of_frames_ << " "
                << (number_of_frames_ == 1 ? "frame" : "frames") << " per message";
}

void RandomTransmitterSatellite::reconfiguring(const Configuration& partial_config) {
    if(partial_config.has("seed")) {
        seed_ = partial_config.get<std::uint8_t>("seed");
        LOG(STATUS) << "Reconfigured seed: " << to_string(seed_);
    }
    if(partial_config.has("frame_size")) {
        frame_size_ = partial_config.get<std::uint64_t>("frame_size");
        LOG(STATUS) << "Reconfigured frame size: " << frame_size_;
    }
    if(partial_config.has("number_of_frames")) {
        number_of_frames_ = partial_config.get<std::uint32_t>("number_of_frames");
        LOG(STATUS) << "Reconfigured number of frames: " << number_of_frames_;
    }
}

void RandomTransmitterSatellite::starting(std::string_view run_identifier) {
    byte_rng_.seed(seed_);
    hwm_reached_ = 0;
    LOG(INFO) << "Starting run " << run_identifier << " with seed " << to_string(seed_);
}

void RandomTransmitterSatellite::running(const std::stop_token& stop_token) {
    while(!stop_token.stop_requested()) {
        auto msg = newDataMessage(number_of_frames_);
        for(std::uint32_t n = 0; n < number_of_frames_; ++n) {
            // Generate random bytes
            std::vector<std::uint8_t> data {};
            data.resize(frame_size_);
            std::ranges::generate(data, std::ref(byte_rng_));
            // Add data to message
            msg.addFrame(std::move(data));
        }
        const auto success = trySendDataMessage(msg);
        if(!success) {
            ++hwm_reached_;
            LOG_N(WARNING, 5) << "Could not send message, skipping...";
        }
    }
}

void RandomTransmitterSatellite::stopping() {
    LOG_IF(WARNING, hwm_reached_ > 0) << "Could not send " << hwm_reached_ << " messages";
}
