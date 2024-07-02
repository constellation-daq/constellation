/**
 * @file
 * @brief Katherine satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <filesystem>
#include <string_view>

#include <katherinexx/katherinexx.hpp>

#include "constellation/satellite/Satellite.hpp"

class KatherineSatellite final : public constellation::satellite::Satellite {
    /**
     * @brief Shutter trigger modes
     */
    enum class ShutterMode {
        POS_EXT = 0,
        NEG_EXT = 1,
        POS_EXT_TIMER = 2,
        NEG_EXT_TIMER = 3,
        AUTO = 4,
    };

    /**
     * @brief Operation modes
     */
    enum class OperationMode {
        TOA_TOT = 0x000,
        TOA = 0x002,
        EVT_ITOT = 0x004,
        MASK = 0x006,
    };

public:
    KatherineSatellite(std::string_view type, std::string_view name);

public:
    void initializing(constellation::config::Configuration& config) override;
    void launching() override;
    void landing() override;
    void reconfiguring(const constellation::config::Configuration& partial_config) override;
    void starting(std::string_view run_identifier) override;
    void stopping() override;
    void running(const std::stop_token& stop_token) override;

private:
    katherine::dacs parse_dacs_file(std::filesystem::path file_path);
    katherine::px_config parse_px_config_file(std::filesystem::path file_path);

    std::string trim(const std::string& str, const std::string& delims = " \t\n\r\v");

private:
    std::shared_ptr<katherine::device> device_;
    katherine::config katherine_config_ {};
};
