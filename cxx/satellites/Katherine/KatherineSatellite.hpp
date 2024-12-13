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
#include <future>
#include <mutex>
#include <string_view>

#include <katherinexx/katherinexx.hpp>

#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/satellite/TransmitterSatellite.hpp"

class KatherineSatellite final : public constellation::satellite::TransmitterSatellite {
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
    void starting(std::string_view run_identifier) override;
    void stopping() override;

    void running(const std::stop_token& stop_token) override;

    void interrupting(constellation::protocol::CSCP::State previous_state) override;
    void failure(constellation::protocol::CSCP::State previous_state) override;

private:
    katherine::dacs parse_dacs_file(const std::filesystem::path& file_path) const;
    katherine::px_config parse_px_config_file(const std::filesystem::path& file_path) const;

    std::vector<std::string> get_hw_info();

    std::string trim(const std::string& str, const std::string& delims = " \t\n\r\v") const;

    void frame_started(int frame_idx);
    void frame_ended(int frame_idx, bool completed, const katherine_frame_info_t& info);

    void data_received(const char* data, size_t count);

    template <typename T> void pixels_received(const T* px, size_t count) {
        auto msg = newDataMessage();
        LOG(TRACE) << "Received buffer with " << count << " pixel hits";
        LOG_IF(TRACE, count > 0) << "First pixel of buffer: " << static_cast<int>(px[0].coord.x) << " "
                                 << static_cast<int>(px[0].coord.y);

        for(size_t i = 0; i < count; ++i) {
            msg.addFrame(to_bytes(px[i]));
        }
        // Try to send and retry if it failed:
        LOG(DEBUG) << "Sending message with " << msg.countFrames() << " pixels";
        sendDataMessage(msg);
    }

    template <typename T> std::vector<std::uint8_t> to_bytes(const T& pixel) {
        std::vector<std::uint8_t> data;

        if constexpr(std::is_same_v<T, katherine::acq::f_toa_tot::pixel_type>) {
            // 2x8b coors, 8b ftoa, 64b toa, 16b tot = 13
            data.reserve(13);
            data.push_back(pixel.coord.x);
            data.push_back(pixel.coord.y);
            data.push_back(pixel.ftoa);
            for(std::size_t i = 0; i < 8; i++) {
                data.push_back((pixel.toa >> (i * 8)) & 0xff);
            }
            data.push_back(pixel.tot & 0xff);
            data.push_back((pixel.tot >> 8) & 0xff);
        }
        return data;
    }

private:
    std::shared_ptr<katherine::device> device_;
    std::mutex katherine_cmd_mutex_;
    std::mutex katherine_acq_mutex_;
    std::mutex katherine_data_mutex_;

    std::shared_ptr<katherine::base_acquisition> acquisition_;
    katherine::config katherine_config_ {};

    /* Future to keep track of Katherine acquisition call state & exceptions */
    std::future<void> acq_future_;

    katherine::readout_type ro_type_ {};
    OperationMode opmode_ {};
    int data_buffer_depth_ {};
    int pixel_buffer_depth_ {};
    bool decode_data_ {};
};
