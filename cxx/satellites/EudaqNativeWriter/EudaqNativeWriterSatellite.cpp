/**
 * @file
 * @brief Implementation of EUDAQ data receiver satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "EudaqNativeWriterSatellite.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/timers.hpp"
#include "constellation/satellite/ReceiverSatellite.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::protocol;
using namespace constellation::satellite;
using namespace constellation::utils;

EudaqNativeWriterSatellite::EudaqNativeWriterSatellite(std::string_view type, std::string_view name)
    : ReceiverSatellite(type, name) {}

void EudaqNativeWriterSatellite::initializing(Configuration& config) {
    allow_overwriting_ = config.get<bool>("allow_overwriting", false);
    base_path_ = config.getPath("output_directory", true);
    flush_timer_ = TimeoutTimer(std::chrono::seconds(config.get<std::size_t>("flush_interval", 3)));
}

void EudaqNativeWriterSatellite::starting(std::string_view run_identifier) {

    // Fetch sequence from run id:
    const auto pos = run_identifier.find_last_of('_');
    std::uint32_t sequence = 0;
    try {
        sequence = (pos != std::string::npos ? std::stoi(std::string(run_identifier).substr(pos + 1)) : 0);
    } catch(std::invalid_argument&) {
        LOG(DEBUG) << "Could not determine run sequence from run identifier, assuming 0";
    }

    // Build target file path:
    const auto file_path = base_path_ / std::filesystem::path("data_" + std::string(run_identifier) + ".raw");

    LOG(STATUS) << "Starting run with identifier " << run_identifier << ", sequence " << sequence;
    serializer_ = std::make_unique<FileSerializer>(file_path, sequence, true);

    // Start timer for flushing data to file
    flush_timer_.reset();
}

void EudaqNativeWriterSatellite::stopping() {
    serializer_.reset();
}

void EudaqNativeWriterSatellite::interrupting(CSCP::State /*previous_state*/) {
    serializer_->flush();
    serializer_.reset();
}

void EudaqNativeWriterSatellite::failure(CSCP::State /*previous_state*/) {
    serializer_.reset();
}

void EudaqNativeWriterSatellite::receive_bor(const CDTP1Message::Header& header, Configuration config) {
    LOG(INFO) << "Received BOR from " << header.getSender() << " with config" << config.getDictionary().to_string();
    serializer_->serializeDelimiterMsg(header, config.getDictionary());
}

void EudaqNativeWriterSatellite::receive_data(CDTP1Message data_message) {
    const auto& header = data_message.getHeader();
    LOG(DEBUG) << "Received data message from " << header.getSender();
    serializer_->serializeDataMsg(data_message);

    // Flush if necessary and reset timer
    if(flush_timer_.timeoutReached()) {
        serializer_->flush();
        flush_timer_.reset();
    }
}

void EudaqNativeWriterSatellite::receive_eor(const CDTP1Message::Header& header, Dictionary run_metadata) {
    LOG(INFO) << "Received EOR from " << header.getSender() << " with metadata" << run_metadata.to_string();
    serializer_->serializeDelimiterMsg(header, run_metadata);
}
