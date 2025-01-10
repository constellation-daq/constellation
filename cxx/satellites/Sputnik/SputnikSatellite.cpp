/**
 * @file
 * @brief Implementation of the Sputnik prototype satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "SputnikSatellite.hpp"

#include <chrono>
#include <cstdint>
#include <stop_token>
#include <string_view>
#include <thread>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/metrics/Metric.hpp"
#include "constellation/core/metrics/stat.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::config;
using namespace constellation::metrics;
using namespace constellation::satellite;
using namespace std::chrono_literals;

SputnikSatellite::SputnikSatellite(std::string_view type, std::string_view name) : Satellite(type, name) {
    LOG(STATUS) << "Sputnik prototype satellite " << getCanonicalName() << " created";
}

void SputnikSatellite::initializing(Configuration& config) {
    // Obtain the beeping interval from the configuration:
    auto interval = config.get<std::uint64_t>("interval", 3000U);

    register_timed_metric(
        "BEEP", "beeps", MetricType::LAST_VALUE, "Sputnik beeps", std::chrono::milliseconds(interval), []() { return 42; });

    register_metric("TIME", "s", MetricType::LAST_VALUE, "Sputnik total running time");
}

void SputnikSatellite::launching() {
    launch_time_ = std::chrono::system_clock::now();
}

void SputnikSatellite::running(const std::stop_token& stop_token) {
    while(!stop_token.stop_requested()) {
        STAT_T("TIME", (std::chrono::system_clock::now() - launch_time_).count() * 1e-9, 10s);
        std::this_thread::sleep_for(50ms);
    }
}
