/**
 * @file
 * @brief Implementation of random data sender satellites
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "RawFileWriterSatellite.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <ios>
#include <optional>
#include <span>
#include <stop_token>
#include <string_view>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/exceptions.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::satellite;
using namespace constellation::utils;

RawFileWriterSatellite::RawFileWriterSatellite(std::string_view type_name, std::string_view satellite_name)
    : Satellite(type_name, satellite_name) {
    support_reconfigure();
}

void RawFileWriterSatellite::initializing(Configuration& config) {
    data_receiver_.initializing(config);

    // Get output directory
    if(config.has("output_directory")) {
        output_directory_ = config.getPath("output_directory", false);
    } else {
        output_directory_ = std::filesystem::current_path();
    }
    if(!std::filesystem::is_directory(output_directory_)) {
        throw SatelliteError(output_directory_.string() + " is not a directory");
    }
    LOG(INFO) << "Writing files to " << output_directory_.string();
}

void RawFileWriterSatellite::launching() {
    data_receiver_.launching();
}

void RawFileWriterSatellite::reconfiguring(const Configuration& partial_config) {
    if(partial_config.has("output_directory")) {
        output_directory_ = partial_config.getPath("output_directory", false);
        if(!std::filesystem::is_directory(output_directory_)) {
            throw SatelliteError(output_directory_.string() + " is not a directory");
        }
        LOG(STATUS) << "Reconfigured output directory: " << output_directory_.string();
    }
    data_receiver_.reconfiguring(partial_config);
}

void RawFileWriterSatellite::starting(std::string_view run_identifier) {
    // Reset bytes written
    bytes_written_ = 0;

    // Open binary file
    const auto file = output_directory_ / ("run_" + to_string(run_identifier) + ".bin");
    if(std::filesystem::exists(file)) {
        LOG(WARNING) << "Overwriting " << file.string();
    }
    file_.open(file, std::ios::out | std::ios::binary);

    // Start time for data rate
    timer_.start();

    // Write config
    const auto config = data_receiver_.starting();
    write_bytes(config.assemble().span());

    LOG(INFO) << "Starting run " << run_identifier << ", writing to " << file.string();
}

void RawFileWriterSatellite::running(const std::stop_token& stop_token) {
    while(!stop_token.stop_requested()) {
        write_message(data_receiver_.recvData());
    }
}

void RawFileWriterSatellite::stopping() {
    // Receive data until EOR
    data_receiver_.stopping();
    while(!data_receiver_.gotEOR()) {
        write_message(data_receiver_.recvData());
    }

    // Write run metadata
    const auto run_metadata = data_receiver_.getEOR();
    write_bytes(run_metadata.assemble().span());

    // Close file
    file_.close();

    // Stop timer
    timer_.stop();

    using namespace std::chrono;
    const auto gb_written = 1e-9 * static_cast<double>(bytes_written_);
    const auto run_duration_ns = duration_cast<nanoseconds>(timer_.duration());
    const auto run_duration_s = duration_cast<seconds>(run_duration_ns);
    LOG(STATUS) << "Stopping run, written " << gb_written << "GB in " << run_duration_s << " ("
                << static_cast<double>(bytes_written_) / static_cast<double>(run_duration_ns.count()) << " GB/s)";
}

void RawFileWriterSatellite::write_message(std::optional<CDTP1Message> msg) {
    if(msg.has_value()) {
        for(const auto& payload : msg.value().getPayload()) {
            write_bytes(payload.span());
        }
    } else {
        // We seem to have time, let's flush
        LOG(TRACE) << "Flushing file...";
        file_.flush();
    }
}

void RawFileWriterSatellite::write_bytes(std::span<const std::byte> bytes) {
    // Write size as 32-bit unsigned integer
    auto size_bytes_u32 = static_cast<std::uint32_t>(bytes.size());
    file_.write(to_char_ptr(&size_bytes_u32), sizeof(size_bytes_u32));
    // Write bytes
    file_.write(to_char_ptr(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    // Increase number of written bytes
    bytes_written_ += sizeof(size_bytes_u32) + bytes.size();
}
