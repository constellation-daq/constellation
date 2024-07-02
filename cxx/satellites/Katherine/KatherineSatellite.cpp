/**
 * @file
 * @brief Implementation of the Katherine satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "KatherineSatellite.hpp"

#include <fstream>
#include <string_view>

#include "constellation/core/logging/log.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::satellite;
using namespace constellation::utils;

KatherineSatellite::KatherineSatellite(std::string_view type, std::string_view name) : Satellite(type, name) {}

void KatherineSatellite::initializing(constellation::config::Configuration& config) {

    // Set default values for the configuration:
    config.setDefault("positive_polarity", true);
    config.setDefault("sequential_mode", false);
    config.setDefault("op_mode", OperationMode::TOA_TOT);
    config.setDefault("shutter_mode", ShutterMode::AUTO);

    auto ip_address = config.get<std::string>("ip_address");
    LOG(DEBUG) << "Attempting to connect to Katherine system at " << ip_address;

    try {
        // Connect to Katherine system, print device ID
        device_ = std::make_shared<katherine::device>(ip_address);

        // Read back information
        LOG(STATUS) << "Katherine's current temperature: " << device_->readout_temperature() << "C";
        LOG(STATUS) << "Katherine reports chip ID: " << device_->chip_id();
    } catch(katherine::system_error& error) {
        throw CommunicationError("Katherine error: " + std::string(error.what()));
    }
}

void KatherineSatellite::launching() {

    const auto config = getConfig();

    LOG(DEBUG) << "Configuring Katherine system";
    katherine_config_ = katherine::config {};

    // Set threshold polarity
    katherine_config_.set_polarity_holes(config.get<bool>("positive_polarity"));

    // Set datadriven or framebased mode:
    auto ro_type =
        (config.get<bool>("sequential_mode") ? katherine::readout_type::sequential : katherine::readout_type::data_driven);

    // Set operation mode of pixel matrix
    auto opmode = config.get<OperationMode>("op_mode");

    // Trigger configuration
    auto trigger_mode = config.get<ShutterMode>("shutter_mode");
    LOG(INFO) << "Configured trigger mode to " << to_string(trigger_mode);

    if(trigger_mode == ShutterMode::AUTO) {
        katherine_config_.set_start_trigger(katherine::no_trigger);
    } else {
        // No "autotriggering"
        // enabled, channel, use_falling_edge
        katherine_config_.set_start_trigger(
            {true, 0, (trigger_mode == ShutterMode::POS_EXT || trigger_mode == ShutterMode::POS_EXT_TIMER) ? false : true});
    }

    if(trigger_mode == ShutterMode::POS_EXT_TIMER || trigger_mode == ShutterMode::NEG_EXT_TIMER) {
        auto trig_width = std::chrono::nanoseconds(config.get<std::uint64_t>("shutter_width"));
        LOG(INFO) << "Shutter length: " << trig_width;
        katherine_config_.set_acq_time(trig_width);
    }

    katherine_config_.set_stop_trigger(katherine::no_trigger);
    katherine_config_.set_delayed_start(false);

    // For now, these constants are hard-coded.
    LOG(DEBUG) << "Configuring bias, clocks and frame setup";
    katherine_config_.set_bias_id(0);
    katherine_config_.set_bias(0);

    // FIXME set number of frames to acquire?
    katherine_config_.set_no_frames(1);

    katherine_config_.set_gray_disable(false);
    katherine_config_.set_phase(katherine::phase::p1);
    katherine_config_.set_freq(katherine::freq::f40);

    // Set the DACs in the Katherine config
    auto dacs = parse_dacs_file(config.getPath("dacs_file"));
    LOG(DEBUG) << "Sending DACs to Katherine system";
    katherine_config_.set_dacs(std::move(dacs));

    auto px_config = parse_px_config_file(config.getPath("px_config_file"));
    katherine_config_.set_pixel_config(std::move(px_config));
}

void KatherineSatellite::landing() {}

void KatherineSatellite::reconfiguring(const constellation::config::Configuration& partial_config) {}

void KatherineSatellite::starting(std::string_view run_identifier) {}

void KatherineSatellite::stopping() {}

void KatherineSatellite::running(const std::stop_token& stop_token) {}

katherine::dacs KatherineSatellite::parse_dacs_file(std::filesystem::path file_path) {

    LOG(DEBUG) << "Attempting to read DAC file at " << file_path;
    std::ifstream dacfile(file_path);
    if(!dacfile.good()) {
        throw SatelliteError("Failed to open DAC file at " + file_path.string());
    }

    LOG(INFO) << "Reading DAC file " << file_path;

    katherine::dacs dacs {};
    std::string line;
    while(std::getline(dacfile, line)) {
        // Trim whitespace and line breaks
        line = trim(line);

        // Ignore empty lines or comments
        if(line.empty() || line.front() == '#') {
            continue;
        }

        std::istringstream iss(line);

        int dac_nr, dac_val;
        if(!(iss >> dac_nr >> dac_val)) {
            LOG(DEBUG) << "Read invalid line: " << line;
            break;
        }

        LOG(DEBUG) << "Setting DAC " << dac_nr << " = " << dac_val;

        // Assign dac value
        dacs.array[dac_nr - 1] = dac_val;
    }
    return dacs;
}

katherine::px_config KatherineSatellite::parse_px_config_file(std::filesystem::path file_path) {

    LOG(INFO) << "Attempting to read pixel configuration file at " << file_path;

    std::ifstream trimfile(file_path);
    if(!trimfile.good()) {
        throw SatelliteError("Failed to open pixel configuration file at " + file_path.string());
    }

    LOG(INFO) << "Reading trimdac file " << file_path;

    // We want to read line by line, pixel by pixel.
    // Katherine expects a array blob of binary data, each pixel has 8bit (char) of data with:
    // 1b: mask, 4b: loc_thl, 1b: testpulse, 2b: reserved
    // We need to end up with 16384 32bit words = 65536 pixel / 4 pixel per word

    // Generate new object and reset memory
    katherine::px_config px_config {};
    memset(&px_config.words, 0, 65536);
    uint32_t* dest = (uint32_t*)px_config.words;

    std::string tline;
    size_t pixels = 0;
    size_t masked = 0;
    size_t tp_enabled = 0;
    while(std::getline(trimfile, tline)) {
        // Trim whitespace and line breaks
        tline = trim(tline);

        // Ignore empty lines or comments
        if(tline.empty() || tline.front() == '#') {
            continue;
        }

        std::istringstream iss(tline);

        int row, col, thr, mask, tp_ena;
        if(!(iss >> col >> row >> thr >> mask >> tp_ena)) {
            LOG(WARNING) << "Read invalid line: " << tline;
            break;
        }

        LOG(TRACE) << "Pixel " << col << ", " << row << ": " << thr << " " << mask << " " << tp_ena;

        // Store to the array - this is harvested from libkatherine's px_config.c:
        auto y = 255 - row;
        uint8_t src = (mask & 0x1) | ((thr & 0xF) << 1) | ((tp_ena & 0x1) << 5); // maybe it's the other way around?
        dest[(64 * col) + (y >> 2)] |= (uint32_t)(src << (8 * (3 - (y % 4))));

        pixels++;
        if(mask) {
            masked++;
        }
        if(tp_ena) {
            tp_enabled++;
        }
    }

    LOG(INFO) << "Read " << pixels << " pixels, " << masked << " masked and " << tp_enabled << " with testpulse enabled";
    return px_config;
}

std::string KatherineSatellite::trim(const std::string& str, const std::string& delims) {
    auto b = str.find_first_not_of(delims);
    auto e = str.find_last_not_of(delims);
    if(b == std::string::npos || e == std::string::npos) {
        return "";
    }
    return {str, b, e - b + 1};
}
