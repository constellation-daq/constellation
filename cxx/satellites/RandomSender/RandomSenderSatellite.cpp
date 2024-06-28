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
    : Satellite(type_name, satellite_name), data_sender_(getCanonicalName()), byte_rng_(generate_random_seed()) {
    support_reconfigure();
}

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

void RandomSenderSatellite::reconfiguring(const Configuration& partial_config) {
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
    data_sender_.reconfiguring(partial_config);
}

void RandomSenderSatellite::starting(std::string_view run_identifier) {
    byte_rng_.seed(seed_);
    hwm_reached_ = 0;
    data_sender_.starting(getConfig());
    LOG(INFO) << "Starting run " << run_identifier << " with seed " << to_string(seed_);
}

void RandomSenderSatellite::running(const std::stop_token& stop_token) {
    while(!stop_token.stop_requested()) {
        auto msg = data_sender_.newDataMessage(number_of_frames_);
        for(std::uint32_t n = 0; n < number_of_frames_; ++n) {
            // Generate random bytes
            std::vector<std::uint8_t> data {};
            data.resize(frame_size_);
            std::generate(data.begin(), data.end(), std::ref(byte_rng_));
            // Add data to message
            msg.addDataFrame(std::move(data));
        }
        const auto success = data_sender_.sendDataMessage(msg);
        if(!success) {
            ++hwm_reached_;
            LOG_N(WARNING, 5) << "Could not send message, skipping...";
        }
    }
}

void RandomSenderSatellite::stopping() {
    data_sender_.stopping();
    LOG_IF(WARNING, hwm_reached_ > 0) << "Could not send " << hwm_reached_ << " messages";
}
