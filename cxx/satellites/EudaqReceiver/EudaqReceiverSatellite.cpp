/**
 * @file
 * @brief Implementation of EUDAQ data receiver satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "EudaqReceiverSatellite.hpp"

#include <chrono>
#include <string_view>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/satellite/ReceiverSatellite.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::satellite;

EudaqReceiverSatellite::EudaqReceiverSatellite(std::string_view type, std::string_view name)
    : ReceiverSatellite(type, name) {
    support_reconfigure();
}

void EudaqReceiverSatellite::starting(std::string_view run_identifier) {

    // Fetch sequence from run id:
    auto pos = run_identifier.find_last_of("_");
    const auto identifier = (pos != std::string::npos ? run_identifier.substr(0, pos) : run_identifier);
    std::uint32_t sequence = 0;
    try {
        sequence = (pos != std::string::npos ? std::stoi(run_identifier.substr(pos + 1)) : 0);
    } catch(std::invalid_argument&) {
    }

    // Build target file path:
    const auto file_path = std::filesystem::path("data_file_") file_path += run_identifier;

    serializer_ = std::make_unique(file_path, descriptor_, sequence);
}

void EudaqReceiverSatellite::stopping() {
    serializer_.reset();
}

void EudaqReceiverSatellite::receive_bor(const CDTP1Message::Header& header, Configuration config) {
    LOG(INFO) << "Received BOR from " << header.getSender() << " with config" << config.getDictionary().to_string();
}

void EudaqReceiverSatellite::receive_data(CDTP1Message&& data_message) {
    serializer_->serialize(std::move(data_message));
}

void EudaqReceiverSatellite::receive_eor(const CDTP1Message::Header& header, Dictionary run_metadata) {
    LOG(INFO) << "Received EOR from " << header.getSender() << " with metadata" << run_metadata.to_string();
}
