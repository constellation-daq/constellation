/**
 * @file
 * @brief Random data sender satellites
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <stop_token>
#include <string_view>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/satellite/data/DataReceiver.hpp"
#include "constellation/satellite/Satellite.hpp"

class RawFileWriterSatellite final : public constellation::satellite::Satellite {
public:
    RawFileWriterSatellite(std::string_view type_name, std::string_view satellite_name);

    void initializing(constellation::config::Configuration& config) final;
    void starting(std::string_view run_identifier) final;
    void running(const std::stop_token& stop_token) final;
    void stopping() final;

private:
    void write_message(std::optional<constellation::message::CDTP1Message> msg);
    void write_bytes(std::span<const std::byte> bytes);

private:
    constellation::data::DataReceiver data_receiver_;
    std::filesystem::path output_directory_;
    std::ofstream file_;
    std::uint64_t bytes_written_ {0};
    std::chrono::system_clock::time_point run_start_;
};
