/**
 * @file
 * @brief Implementation of Caribou Satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "CaribouSatellite.hpp"

#include <chrono>
#include <thread>

// Caribou Peary includes
#include "utils/configuration.hpp"

#include "constellation/core/logging/log.hpp"

using namespace caribou;
using namespace std::literals::chrono_literals;

CaribouSatellite::CaribouSatellite(std::string_view name) : Satellite(name) {

    // Separate the device name from the device identifier:
    auto const pos = name.find_last_of('_');
    if(pos == std::string::npos) {
        name_ = name;
        LOG(logger_, INFO) << "Instantiated CaribouProducer for device \"" << name << "\"";
    } else {
        name_ = name.substr(0, pos);
        const auto identifier = name.substr(pos + 1);
        LOG(logger_, INFO) << "Instantiated CaribouProducer for device \"" << name << "\" with identifier \"" << identifier
                           << "\"";
    }

    // Create new Peary device manager
    manager_ = new DeviceManager();
}

void CaribouSatellite::initializing(const std::stop_token& stop_token, const std::any& config) {
    LOG(logger_, INFO) << "Initialising CaribouProducer";
    auto ini = GetInitConfiguration();

    // Open configuration file and create object:
    caribou::Configuration config;
    auto confname = ini->Get("config_file", "");
    std::ifstream file(confname);
    LOG(logger_, INFO) << "Attempting to use initial device configuration \"" << confname << "\"";
    if(!file.is_open()) {
        LOG(logger_, CRITICAL) << "No configuration file provided.";
    } else {
        config = caribou::Configuration(file);
    }

    // Select section from the configuration file relevant for this device:
    auto sections = config.GetSections();
    if(std::find(sections.begin(), sections.end(), name_) != sections.end()) {
        config.SetSection(name_);
    }

    std::lock_guard<std::mutex> lock {device_mutex_};
    size_t device_id = manager_->addDevice(name_, config);
    LOG(logger_, INFO) << "Manager returned device ID " << device_id << ", fetching device...";
    device_ = manager_->getDevice(device_id);

    // Add secondary device if it is configured:
    if(ini->Has("secondary_device")) {
        std::string secondary = ini->Get("secondary_device", std::string());
        if(std::find(sections.begin(), sections.end(), secondary) != sections.end()) {
            config.SetSection(secondary);
        }
        size_t device_id2 = manager_->addDevice(secondary, config);
        LOG(logger_, INFO) << "Manager returned device ID " << std::to_string(device_id2)
                           << ", fetching secondary device...";
        secondary_device_ = manager_->getDevice(device_id2);
    }
}

void CaribouSatellite::launching(const std::stop_token& stop_token) {
    auto config = GetConfiguration();
    LOG(logger_, INFO) << "Configuring CaribouProducer: " << config->Name();

    std::lock_guard<std::mutex> lock {device_mutex_};
    LOG(logger_, INFO) << "Configuring device " << device_->getName();

    // Switch on the device power:
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
    if(config->Has("register_key") || config->Has("register_value")) {
        auto key = config->Get("register_key", "");
        auto value = config->Get("register_value", 0);
        device_->setRegister(key, value);
        LOG(logger_, INFO) << "Setting " << key << " = " << std::to_string(value);
    }

    // Select which ADC signal to regularly fetch:
    adc_signal_ = config->Get("adc_signal", "");
    adc_freq_ = config->Get("adc_frequency", 1000);

    if(!adc_signal_.empty()) {
        // Try it out directly to catch misconfiugration
        auto adc_value = device_->getADC(adc_signal_);
        LOG(logger_, INFO) << "Will probe ADC signal \"" << adc_signal_ << "\" every " << std::to_string(adc_freq_)
                           << " events";
    }

    LOG(logger_, STATUS) << "CaribouProducer configured. Ready to start run.";
}

void CaribouSatellite::landing(const std::stop_token& stop_token) {

    std::lock_guard<std::mutex> lock {device_mutex_};
    LOG(logger_, INFO) << "Configuring device " << device_->getName();

    // Switch off the device power:
    device_->powerOff();
}

void CaribouSatellite::reconfiguring(const std::stop_token& stop_token, const std::any& partial_config) {}

void CaribouSatellite::starting(const std::stop_token& stop_token, std::uint32_t run_number) {
    m_ev = 0;

    LOG(logger_, INFO) << "Starting run...";

    // Start the DAQ
    std::lock_guard<std::mutex> lock {device_mutex_};

    // How to add additional information for the Begin-of-run event, e.g. containing tags with detector information?
    // event->SetTag("software", device_->getVersion());
    // event->SetTag("firmware", device_->getFirmwareVersion());
    // event->SetTag("timestamp", LOGTIME);
    // auto registers = device_->getRegisters();
    // for(const auto& reg : registers) {
    // event->SetTag(reg.first, reg.second);
    // }

    // Start DAQ:
    device_->daqStart();

    LOG(logger_, INFO) << "Started run.";
}

void CaribouSatellite::stopping(const std::stop_token& stop_token) {

    LOG(logger_, INFO) << "Stopping run...";

    // Stop the DAQ
    std::lock_guard<std::mutex> lock {device_mutex_};
    device_->daqStop();
    LOG(logger_, INFO) << "Stopped run.";
}

void CaribouSatellite::running(const std::stop_token& stop_token) {

    LOG(logger_, INFO) << "Starting run loop...";
    std::lock_guard<std::mutex> lock {device_mutex_};

    while(!stop_token.stop_requested()) {
        try {
            // Retrieve data from the device:
            auto data = device_->getRawData();

            if(!data.empty()) {
                // Create new event
                auto event = Event::MakeUnique("Caribou" + name_ + "Event");
                // Set event ID
                event->SetEventN(m_ev);
                // Add data to the event
                event->AddBlock(0, data);

                // Query ADC if wanted:
                if(m_ev % adc_freq_ == 0) {
                    if(!adc_signal_.empty()) {
                        auto adc_value = device_->getADC(adc_signal_);
                        LOG(logger_, DEBUG) << "ADC reading: " << adc_signal_ << " =  " << std::to_string(adc_value);
                        event->SetTag(adc_signal_, adc_value);
                    }
                }

                // We do not want to generate sub-events - send the event directly off to the Data Collector
                SendEvent(std::move(event));
            }

            // Now increment the event number
            m_ev++;

            LOG(logger_, DEBUG) << "Frame " << m_ev;
        } catch(caribou::NoDataAvailable&) {
            continue;
        } catch(caribou::DataException& e) {
            // Retrieval failed, retry once more before aborting:
            LOG(logger_, WARNING) << e.what() << ", skipping data packet";
            continue;
        } catch(caribou::caribouException& e) {
            // FIXME sat exceptions missing.
            LOG(logger_, CRITICAL) << e.what();
            throw;
        }
    }

    LOG(logger_, INFO) << "Exiting run loop.";
}
