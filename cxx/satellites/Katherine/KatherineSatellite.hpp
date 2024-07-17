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
#include <thread>

#include <katherinexx/katherinexx.hpp>

#include "constellation/core/log/log.hpp"
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
    void starting(std::string_view run_identifier) override;
    void stopping() override;
    void running(const std::stop_token& stop_token) override;

private:
    katherine::dacs parse_dacs_file(std::filesystem::path file_path);
    katherine::px_config parse_px_config_file(std::filesystem::path file_path);

    std::string trim(const std::string& str, const std::string& delims = " \t\n\r\v");

    void frame_started(int frame_idx);
    void frame_ended(int frame_idx, bool completed, const katherine_frame_info_t& info);

    template <typename T> void pixels_received(const T* px, size_t count) {
        for(size_t i = 0; i < count; ++i) {
            if constexpr(std::is_same_v<T, katherine::acq::f_toa_tot::pixel_type>) {
                LOG(TRACE) << (int)px[i].coord.x << " " << (int)px[i].coord.y;
            }
        }
    }

private:
    std::shared_ptr<katherine::device> device_;
    std::shared_ptr<katherine::base_acquisition> acquisition_;
    katherine::config katherine_config_ {};
    std::thread runthread_;
    katherine::readout_type ro_type_ {};
    OperationMode opmode_ {};
};
