/**
 * @file
 * @brief Implementation of Caribou Satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "CaribouSatellite.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>

// Caribou Peary includes
#include <peary/device/DeviceManager.hpp>
#include <peary/utils/configuration.hpp>
#include <peary/utils/exceptions.hpp>

#include "constellation/core/logging/log.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace caribou;
using namespace std::literals::chrono_literals;

// generator function for loading satellite from shared library
extern "C" std::shared_ptr<Satellite> generator(std::string_view type_name, std::string_view satellite_name) {
    return std::make_shared<CaribouSatellite>(type_name, satellite_name);
}

CaribouSatellite::CaribouSatellite(std::string_view type_name, std::string_view satellite_name)
    : Satellite(type_name, satellite_name), manager_(std::make_shared<DeviceManager>()) {}

void CaribouSatellite::initializing(constellation::config::Configuration& config) {
    LOG(logger_, INFO) << "Initializing " << getCanonicalName();

    // Clear all existing devices - the initializing method can be called multiple times!
    manager_->clearDevices();

    // Read the device type from the configuration:
    device_class_ = config.get<std::string>("type");
    LOG(logger_, INFO) << "Instantiated " << getCanonicalName() << " for device \"" << device_class_ << "\"";

    // Open configuration file and read caribou configuration
    caribou::Configuration caribou_config {};
    const auto config_file_path = config.getPath("config_file");
    LOG(logger_, INFO) << "Attempting to use initial device configuration \"" << config_file_path << "\"";
    std::ifstream config_file {config_file_path};
    if(!config_file.is_open()) {
        LOG(logger_, CRITICAL) << "Could not open configuration file \"" << config_file_path << "\"";
    } else {
        caribou_config = caribou::Configuration(config_file);
    }

    // Select section from the configuration file relevant for this device
    const auto sections = caribou_config.GetSections();
    if(std::find(sections.cbegin(), sections.cend(), device_class_) != sections.cend()) {
        caribou_config.SetSection(device_class_);
    }

    std::lock_guard<std::mutex> lock {device_mutex_};
    try {
        const auto device_id = manager_->addDevice(device_class_, caribou_config);
        LOG(logger_, INFO) << "Manager returned device ID " << device_id << ", fetching device...";
        device_ = manager_->getDevice(device_id);
    } catch(const caribou::DeviceException& error) {
        LOG(logger_, CRITICAL) << "Failed to get device \"" << device_class_ << "\": " << error.what();
        throw std::exception();
    }

    // Add secondary device if it is configured
    if(config.has("secondary_device")) {
        const auto secondary = config.get<std::string>("secondary_device");
        if(std::find(sections.begin(), sections.end(), secondary) != sections.end()) {
            caribou_config.SetSection(secondary);
        }
        try {
            const auto device_id2 = manager_->addDevice(secondary, caribou_config);
            LOG(logger_, INFO) << "Manager returned device ID " << device_id2 << ", fetching secondary device...";
            secondary_device_ = manager_->getDevice(device_id2);
        } catch(const caribou::DeviceException& error) {
            LOG(logger_, CRITICAL) << "Failed to get secondary device \"" << secondary << "\": " << error.what();
            throw std::exception();
        }
    }

    // Store the configuration:
    config_ = config;
    LOG(logger_, STATUS) << getCanonicalName() << " initialized";
}

void CaribouSatellite::launching() {
    LOG(logger_, INFO) << "Configuring device " << device_->getName();

    std::lock_guard<std::mutex> lock {device_mutex_};

    // Switch on the device power
    device_->powerOn();

    if(secondary_device_ != nullptr) {
        secondary_device_->powerOn();
    }

    // Wait for power to stabilize and for the TLU clock to be present
    std::this_thread::sleep_for(1s);

    // Configure the device
    device_->configure();

    if(secondary_device_ != nullptr) {
        secondary_device_->configure();
    }

    // Set additional registers from the configuration:
    if(config_.has("register_key") || config_.has("register_value")) {
        auto key = config_.get<std::string>("register_key", "");
        auto value = config_.get("register_value", 0);
        device_->setRegister(key, value);
        LOG(logger_, INFO) << "Setting " << key << " = " << std::to_string(value);
    }

    // Select which ADC signal to regularly fetch:
    adc_signal_ = config_.get<std::string>("adc_signal", "");
    adc_freq_ = config_.get("adc_frequency", 1000);

    if(!adc_signal_.empty()) {
        // Try it out directly to catch misconfiugration
        auto adc_value = device_->getADC(adc_signal_);
        LOG(logger_, INFO) << "Will probe ADC signal \"" << adc_signal_ << "\" every " << adc_freq_ << " frames";
        LOG(logger_, TRACE) << "ADC value: " << adc_value; // FIXME: unused variable, send as stats instead
    }

    LOG(logger_, STATUS) << getCanonicalName() << " launched";
}

void CaribouSatellite::landing() {
    LOG(logger_, INFO) << "Switching off power for device " << device_->getName();

    // Switch off the device power
    std::lock_guard<std::mutex> lock {device_mutex_};
    device_->powerOff();

    LOG(logger_, STATUS) << getCanonicalName() << " landed";
}

void CaribouSatellite::reconfiguring(const constellation::config::Configuration& /*partial_config*/) {}

void CaribouSatellite::starting(std::uint32_t run_number) {
    LOG(logger_, INFO) << "Starting run " << run_number << "...";

    // Reset frame number
    frame_nr_ = 0;

    // Start the DAQ
    std::lock_guard<std::mutex> lock {device_mutex_};

    // How to add additional information for the Begin-of-run event, e.g. containing tags with detector information?
    /*DataSender::BORMessage bor_msg {};
    bor_msg.set_config(config_);
    bor_msg.add_tag("software", device_->getVersion());
    bor_msg.add_tag("firmware", device_->getFirmwareVersion());
    const auto registers = device_->getRegisters();
    for(const auto& reg : registers) {
        bor_msg.add_tag(reg.first, reg.second);
    }
    data_sender_.sendBOR(std::move(bor_msg));*/

    // Start DAQ:
    device_->daqStart();

    LOG(logger_, STATUS) << getCanonicalName() << " started (run " << run_number << ")";
}

void CaribouSatellite::stopping() {
    LOG(logger_, INFO) << "Stopping run...";

    // Stop the DAQ
    std::lock_guard<std::mutex> lock {device_mutex_};
    device_->daqStop();

    LOG(logger_, STATUS) << getCanonicalName() << " stopped";
}

void CaribouSatellite::running(const std::stop_token& stop_token) {
    LOG(logger_, INFO) << "Starting run loop...";

    std::lock_guard<std::mutex> lock {device_mutex_};

    while(!stop_token.stop_requested()) {
        try {
            // Retrieve data from the device
            LOG(logger_, TRACE) << "Trying to receive data from device";
            const auto data = device_->getRawData();

            LOG(logger_, DEBUG) << "Frame " << frame_nr_;

            if(!data.empty()) {
                // Create new data message for data sender
                /*DataSender::Message msg {};

                // Set frame number
                msg.addTag("frame", frame_nr_);

                // Add data
                msg.addDataFrame(std::move(data));

                // Query ADC if wanted:
                if(frame_nr_ % adc_freq_ == 0) {
                    if(!adc_signal_.empty()) {
                        auto adc_value = device_->getADC(adc_signal_);
                        LOG(logger_, DEBUG) << "ADC reading: " << adc_signal_ << " =  " << std::to_string(adc_value);
                        msg.addTag(adc_signal_, adc_value);
                    }
                }

                // Send data (async)
                data_sender_.sendData(std::move(msg));*/
            }

            // Now increment the frame number
            frame_nr_++;
        } catch(caribou::NoDataAvailable&) {
            continue;
        } catch(caribou::DataException& e) {
            // Retrieval failed, retry once more before aborting:
            LOG(logger_, WARNING) << e.what() << ", skipping data packet";
            continue;
        } catch(caribou::caribouException& e) {
            // FIXME sat exceptions missing.
            LOG(logger_, CRITICAL) << e.what();
            throw std::exception();
        }
    }

    LOG(logger_, INFO) << "Exiting run loop";
}
