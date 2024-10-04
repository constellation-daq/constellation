/**
 * @file
 * @brief Implementation of the Flight Recorder satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "FlightRecorderSatellite.hpp"

#include <string_view>

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include "constellation/core/log/log.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::chirp;
using namespace constellation::config;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::satellite;

FlightRecorderSatellite::FlightRecorderSatellite(std::string_view type, std::string_view name)
    : Satellite(type, name), SubscriberPool<CMDP1LogMessage, MONITORING>(
                                 "LOGRECV", [this](auto&& arg) { add_message(std::forward<decltype(arg)>(arg)); }) {
    // Start the log receiver pool
    startPool();
}

void FlightRecorderSatellite::initializing(Configuration& config) {
    global_level_ = config.get<Level>("global_recording_level", WARNING);

    try {
        file_logger_ = spdlog::basic_logger_mt("file_logger", "/tmp/basic-log.txt");
        spdlog::flush_every(std::chrono::seconds(1));
    } catch(const spdlog::spdlog_ex& ex) {
        throw SatelliteError(ex.what());
    }

    std::string global_topic = "LOG/";
    global_topic += magic_enum::enum_name(global_level_);
    // subscribe for all endpoints to global topic:
    subscribe(global_topic);
}

void FlightRecorderSatellite::add_message(CMDP1LogMessage&& msg) {
    LOG(WARNING) << "Logging: " << msg.getLogMessage();
    if(file_logger_) {
        file_logger_->info(msg.getLogMessage());
    }
}
