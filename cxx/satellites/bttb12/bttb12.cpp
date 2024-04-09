/**
 * @file
 * @brief Implementation of the BTTB12 satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "bttb12.hpp"

#include <chrono>
#include <memory>
#include <string_view>
#include <thread>

#include "constellation/core/logging/log.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::satellite;
using namespace std::literals::chrono_literals;

// generator function for loading satellite from shared library
// TODO(stephan.lachnit): hide away in build system
extern "C" std::shared_ptr<Satellite> generator(std::string_view type_name, std::string_view satellite_name) {
    return std::make_shared<bttb12>(type_name, satellite_name);
}

bttb12::bttb12(std::string_view type_name, std::string_view satellite_name) : Satellite(type_name, satellite_name) {}

std::vector<std::uint64_t> bttb12::getDataFromDAQ() {
    std::this_thread::sleep_for(1s);
    return {};
}

void sendData(std::vector<uint64_t>) {}

void bttb12::starting(std::uint32_t run_number) {
    frame_number_ = 0;
    LOG(logger_, STATUS) << "Starting run " << run_number;
}

void bttb12::running(const std::stop_token& stop_token) {
    while(!stop_token.stop_requested()) {
        auto data = getDataFromDAQ();
        LOG(logger_, STATUS) << "Got frame " << frame_number_;
        sendData(data);
        frame_number_++;
    }
}
