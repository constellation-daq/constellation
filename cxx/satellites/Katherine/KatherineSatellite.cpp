/**
 * @file
 * @brief Implementation of the Katherine satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "KatherineSatellite.hpp"

#include <condition_variable>
#include <fstream>
#include <string_view>

#include "constellation/core/logging/log.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::satellite;
using namespace constellation::utils;
using namespace std::literals::chrono_literals;

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
        // If we already have a device connected, remove it - we might be calling initialize multiple times!
        if(device_) {
            device_.reset();
        }

        // Connect to Katherine system, print device ID
        device_ = std::make_shared<katherine::device>(ip_address);

        // Read back information
        LOG(STATUS) << "Katherine's current temperature: " << device_->readout_temperature() << "C";
        LOG(STATUS) << "Katherine reports chip ID: " << device_->chip_id();
    } catch(katherine::system_error& error) {
        throw CommunicationError("Katherine error: " + std::string(error.what()));
    }

    LOG(DEBUG) << "Configuring Katherine system";
    katherine_config_ = katherine::config {};

    // Set threshold polarity
    katherine_config_.set_polarity_holes(config.get<bool>("positive_polarity"));

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

    // Set datadriven or framebased mode:
    ro_type_ =
        (config.get<bool>("sequential_mode") ? katherine::readout_type::sequential : katherine::readout_type::data_driven);
    opmode_ = config.get<OperationMode>("op_mode");
}

void KatherineSatellite::launching() {
    // If we are in data-driven mode, disable timeout:
    auto timeout = (ro_type_ == katherine::readout_type::data_driven) ? -1s : 10s;

    // Select acquisition mode and create acquisition object
    if(opmode == OperationMode::TOA_TOT) {
        auto acq = std::make_shared<katherine::acquisition<katherine::acq::f_toa_tot>>(
            *device_, katherine::md_size * 34952533, sizeof(katherine::acq::f_toa_tot::pixel_type) * 65536, 500ms, timeout);
        acq->set_pixels_received_handler(
            std::bind_front(&KatherineSatellite::pixels_received<katherine::acq::f_toa_tot::pixel_type>, this));
        acquisition_ = acq;
    } else if(opmode == OperationMode::TOA) {
        auto acq = std::make_shared<katherine::acquisition<katherine::acq::f_toa_only>>(
            *device_, katherine::md_size * 34952533, sizeof(katherine::acq::f_toa_only::pixel_type) * 65536, 500ms, timeout);
        acq->set_pixels_received_handler(
            std::bind_front(&KatherineSatellite::pixels_received<katherine::acq::f_toa_only::pixel_type>, this));
        acquisition_ = acq;
    } else if(opmode == OperationMode::EVT_ITOT) {
        auto acq = std::make_shared<katherine::acquisition<katherine::acq::f_event_itot>>(
            *device_,
            katherine::md_size * 34952533,
            sizeof(katherine::acq::f_event_itot::pixel_type) * 65536,
            500ms,
            timeout);
        acq->set_pixels_received_handler(
            std::bind_front(&KatherineSatellite::pixels_received<katherine::acq::f_event_itot::pixel_type>, this));
        acquisition_ = acq;
    }

    acquisition_->set_frame_started_handler(std::bind_front(&KatherineSatellite::frame_started, this));
    acquisition_->set_frame_ended_handler(std::bind_front(&KatherineSatellite::frame_ended, this));
}

void KatherineSatellite::landing() {
  if(acquisition_) {
    acquisition_.reset();
  }
}

void KatherineSatellite::frame_started(int frame_idx) {
    LOG(DEBUG) << "Started frame " << frame_idx << std::endl;
}

void KatherineSatellite::frame_ended(int frame_idx, bool completed, const katherine_frame_info_t& info) {
    const double recv_perc = 100. * info.received_pixels / info.sent_pixels;

    LOG(DEBUG) << "Ended frame " << frame_idx << "." << std::endl;
    LOG(DEBUG) << " - tpx3->katherine lost " << info.lost_pixels << " pixels" << std::endl
               << " - katherine->pc sent " << info.sent_pixels << " pixels" << std::endl
               << " - katherine->pc received " << info.received_pixels << " pixels (" << recv_perc << " %)" << std::endl
               << " - state: " << (completed ? "completed" : "not completed") << std::endl
               << " - start time: " << info.start_time.d << std::endl
               << " - end time: " << info.end_time.d << std::endl;
}

void KatherineSatellite::starting(std::string_view) {

    // This needs to be called *before* we start the run thread with acquitions->read
    // otherwise the read function falls through directly with an error
    acquisition_->begin(katherine_config_, ro_type_);

    // Start Katherine run thread:
    runthread_ = std::thread([this]() {
        try {
            acquisition_->read();
        } catch(katherine::system_error& e) {
            LOG(CRITICAL) << "Katherine error: " << e.what();
            if(!acquisition_->aborted()) {
                LOG(STATUS) << "Aborting Katherine run...";
                acquisition_->abort();
            }
        }
    });
    runthread_.detach();
}

void KatherineSatellite::running(const std::stop_token& stop_token) {
  

  // Start the data acquisition:
  while(!stop_token.stop_requested()) {
    std::this_thread::sleep_for(2s);

    LOG(INFO) << "State:   " << katherine::str_acq_state(acquisition_->state());
    LOG(INFO) << "dropped: " << acquisition_->dropped_measurement_data();

  }
}

void KatherineSatellite::stopping() {
    // End the acquisition:
    acquisition_->abort();

    // Join thread once it is done
    if(runthread_.joinable()) {
      runthread_.join();
    }

    LOG(STATUS) << "Acquisition completed:" << std::endl
                << "state: " << katherine::str_acq_state(acquisition_->state()) << std::endl
                << "received " << acquisition_->completed_frames() << " complete frames" << std::endl
                << "dropped " << acquisition_->dropped_measurement_data() << " measurement data items";
}

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
