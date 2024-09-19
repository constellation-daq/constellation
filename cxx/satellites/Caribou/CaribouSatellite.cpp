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
#include <source_location>
#include <sstream>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>

// Caribou Peary includes
#include <peary/device/DeviceManager.hpp>
#include <peary/utils/configuration.hpp>
#include <peary/utils/exceptions.hpp>
#include <peary/utils/logging.hpp>

#include "constellation/core/log/Level.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/satellite/exceptions.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::log;
using namespace constellation::protocol;
using namespace constellation::satellite;
using namespace std::literals::chrono_literals;

Level PearyLogger::getLogLevel(char short_log_format_char) {
    Level level {};
    switch(short_log_format_char) {
    case 'T': level = TRACE; break;
    case 'D': level = DEBUG; break;
    case 'I': level = INFO; break;
    case 'W': level = WARNING; break;
    case 'E': level = CRITICAL; break;
    case 'S': level = STATUS; break;
    case 'F': level = CRITICAL; break;
    default: break;
    }
    return level;
}

PearyLogger::PearyLogger() : logger_("PEARY") {
    // Create static stream for Peary
    static std::ostream stream {this};
    // Set short log format to filter log level
    caribou::Log::setFormat(caribou::LogFormat::SHORT);
    // Add stream to peary
    caribou::Log::addStream(stream);
}

PearyLogger::~PearyLogger() {
    // Delete log streams since logger goes out of scope
    caribou::Log::clearStreams();
}

int PearyLogger::sync() {
    const auto message = this->view();
    // Get log level from message
    const auto level = getLogLevel(message.at(1));
    // Strip log level and final newline from substring
    const auto message_stripped = message.substr(4, message.size() - 5);
    // Log stripped message
    logger_.log(level, std::source_location()) << message_stripped;
    logger_.flush();
    // Reset stringbuf by swapping a new one
    std::stringbuf new_stringbuf {};
    this->swap(new_stringbuf);

    return 0;
}

CaribouSatellite::CaribouSatellite(std::string_view type, std::string_view name)
    : TransmitterSatellite(type, name), manager_(std::make_shared<caribou::DeviceManager>()) {

    // This satellite supports reconfiguration:
    support_reconfigure();

    // Custom Caribou commands for this satellite
    register_command(
        "peary_verbosity", "Set verbosity of the Peary logger.", {}, std::function<void(const std::string&)>([](auto level) {
            caribou::Log::setReportingLevel(caribou::Log::getLevelFromString(level));
        }));
    register_command(
        "get_peary_verbosity",
        "Get the currently configured verbosity of the Peary logger.",
        {},
        std::function<std::string()>([]() { return caribou::Log::getStringFromLevel(caribou::Log::getReportingLevel()); }));
    register_command("list_registers",
                     "List all available register names for the attached Caribou device.",
                     {CSCP::State::INIT, CSCP::State::ORBIT, CSCP::State::RUN},
                     std::function<std::vector<std::string>()>([&]() {
                         std::lock_guard<std::mutex> lock {device_mutex_};
                         return device_->listRegisters();
                     }));
    register_command("list_memories",
                     "List all memory registers for the attached Caribou device.",
                     {CSCP::State::INIT, CSCP::State::ORBIT, CSCP::State::RUN},
                     std::function<std::vector<std::string>()>([&]() {
                         std::lock_guard<std::mutex> lock {device_mutex_};
                         return device_->listMemories();
                     }));
    register_command("get_voltage",
                     "Get selected output voltage (in V) of the attached Caribou device. Provide voltage name as parameter.",
                     {CSCP::State::INIT, CSCP::State::ORBIT, CSCP::State::RUN},
                     std::function<double(const std::string&)>([&](auto name) {
                         std::lock_guard<std::mutex> lock {device_mutex_};
                         return device_->getVoltage(name);
                     }));
    register_command("get_current",
                     "Get selected output current (in A) of the attached Caribou device. Provide current name as parameter.",
                     {CSCP::State::INIT, CSCP::State::ORBIT, CSCP::State::RUN},
                     std::function<double(const std::string&)>([&](auto name) {
                         std::lock_guard<std::mutex> lock {device_mutex_};
                         return device_->getCurrent(name);
                     }));
    register_command("get_power",
                     "Get selected output power (in W) of the attached Caribou device. Provide power name as parameter.",
                     {CSCP::State::INIT, CSCP::State::ORBIT, CSCP::State::RUN},
                     std::function<double(const std::string&)>([&](auto name) {
                         std::lock_guard<std::mutex> lock {device_mutex_};
                         return device_->getPower(name);
                     }));
    register_command("get_register",
                     "Read the value of a register on the attached Caribou device. Provide register name as parameter.",
                     {CSCP::State::INIT, CSCP::State::ORBIT, CSCP::State::RUN},
                     std::function<uintptr_t(const std::string&)>([&](auto name) {
                         std::lock_guard<std::mutex> lock {device_mutex_};
                         return device_->getRegister(name);
                     }));
    register_command("get_memory",
                     "Read the value of a FPGA memory register on the attached Caribou device. Provide memory register "
                     "name as parameter.",
                     {CSCP::State::INIT, CSCP::State::ORBIT, CSCP::State::RUN},
                     std::function<uintptr_t(const std::string&)>([&](auto name) {
                         std::lock_guard<std::mutex> lock {device_mutex_};
                         return device_->getMemory(name);
                     }));
    register_command("get_adc",
                     "Read the voltage from the Carboard ADC (in V) via the attached Caribou device. Provide the "
                     "voltage name as string.",
                     {CSCP::State::INIT, CSCP::State::ORBIT, CSCP::State::RUN},
                     std::function<double(const std::string&)>([&](auto name) {
                         std::lock_guard<std::mutex> lock {device_mutex_};
                         return device_->getADC(name);
                     }));
}

void CaribouSatellite::initializing(constellation::config::Configuration& config) {

    // Set default values:
    config.setDefault("adc_frequency", 1000);
    config.setDefault("peary_verbosity", "INFO");
    config.setDefault("number_of_frames", 1);

    // Clear all existing devices - the initializing method can be called multiple times!
    manager_->clearDevices();

    // Read the device type from the configuration:
    device_class_ = config.get<std::string>("type");
    LOG(INFO) << "Instantiated " << getCanonicalName() << " for device \"" << device_class_ << "\"";

    // Set configured log level
    caribou::Log::setReportingLevel(caribou::Log::getLevelFromString(config.get<std::string>("peary_verbosity")));

    // Open configuration file and read caribou configuration
    const auto config_file_path = config.getPath("config_file");
    LOG(INFO) << "Attempting to use initial device configuration " << config_file_path;
    std::ifstream config_file {config_file_path};
    if(!config_file.is_open()) {
        throw SatelliteError("Could not open configuration file \"" + config_file_path.string() + "\"");
    }
    auto caribou_configs = caribou::ConfigParser(config_file);

    // Select section from the configuration file relevant for this device
    if(!caribou_configs.Has(device_class_)) {
        throw SatelliteError("Could not find section for device \"" + device_class_ + "\" in config file \"" +
                             config_file_path.string() + "\"");
    }
    const auto device_config = caribou_configs.GetConfig(device_class_);

    std::lock_guard<std::mutex> lock {device_mutex_};
    try {
        const auto device_id = manager_->addDevice(device_class_, device_config);
        LOG(INFO) << "Manager returned device ID " << device_id << ", fetching device...";
        device_ = manager_->getDevice(device_id);
    } catch(const caribou::DeviceException& error) {
        throw SatelliteError("Failed to get device \"" + device_class_ + "\": " + error.what());
    }

    // Add secondary device if it is configured
    if(config.has("secondary_device")) {
        const auto secondary = config.get<std::string>("secondary_device");
        const auto secondary_config =
            (caribou_configs.Has(secondary) ? caribou_configs.GetConfig(secondary) : caribou::Configuration {});
        try {
            const auto device_id2 = manager_->addDevice(secondary, secondary_config);
            LOG(INFO) << "Manager returned device ID " << device_id2 << ", fetching secondary device...";
            secondary_device_ = manager_->getDevice(device_id2);
        } catch(const caribou::DeviceException& error) {
            throw SatelliteError("Failed to get secondary device \"" + secondary + "\": " + error.what());
        }
    }

    // Prepare ADC info to be distributed as metrics
    if(config.has("adc_signal")) {
        // Select which ADC signal to regularly fetch:
        adc_signal_ = config.get<std::string>("adc_signal");
        adc_freq_ = config.get<std::uint64_t>("adc_frequency");

        // Try it out directly to catch mis-configuration
        auto adc_value = device_->getADC(adc_signal_);
        LOG(INFO) << "Will probe ADC signal \"" << adc_signal_ << "\" every " << adc_freq_ << " frames";
        LOG(TRACE) << "ADC value: " << adc_value; // FIXME: unused variable, send as stats instead
    }

    // Cache the number of frames to attach to a single data message:
    number_of_frames_ = config.get<std::size_t>("number_of_frames");
}

void CaribouSatellite::launching() {
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
}

void CaribouSatellite::landing() {
    LOG(INFO) << "Switching off power for device " << device_->getName();

    // Switch off the device power
    std::lock_guard<std::mutex> lock {device_mutex_};
    device_->powerOff();
}

void CaribouSatellite::reconfiguring(const constellation::config::Configuration& partial_config) {

    std::lock_guard<std::mutex> lock {device_mutex_};

    // Try reconfiguring registers
    for(const auto& reg : device_->listRegisters()) {
        if(partial_config.has(reg)) {
            const auto value = partial_config.get<uintptr_t>(reg);
            LOG(DEBUG) << "Setting register " << std::quoted(reg) << " to " << value;
            device_->setRegister(reg, value);
        }
    }

    // Try reconfiguring memory registers:
    for(const auto& mem : device_->listMemories()) {
        if(partial_config.has(mem)) {
            const auto value = partial_config.get<uintptr_t>(mem);
            LOG(DEBUG) << "Setting memory register " << std::quoted(mem) << " to " << value;
            device_->setMemory(mem, value);
        }
    }
}

void CaribouSatellite::starting(std::string_view) {

    // Start the DAQ
    std::lock_guard<std::mutex> lock {device_mutex_};

    // How to add additional information for the Begin-of-run event, e.g. containing tags with detector information?
    /*DataSender::BORMessage bor_msg {};
    bor_msg.add_tag("software", device_->getVersion());
    bor_msg.add_tag("firmware", device_->getFirmwareVersion());
    const auto registers = device_->getRegisters();
    for(const auto& reg : registers) {
        bor_msg.add_tag(reg.first, reg.second);
    }
    data_sender_.sendBOR(std::move(bor_msg));*/

    // Start DAQ:
    device_->daqStart();
}

void CaribouSatellite::stopping() {
    // Stop the DAQ
    std::lock_guard<std::mutex> lock {device_mutex_};
    device_->daqStop();
}

void CaribouSatellite::running(const std::stop_token& stop_token) {

    // Keep track of current number of frames:
    std::size_t current_frames = 0;

    // Prepare data message
    auto msg = newDataMessage(number_of_frames_);

    while(!stop_token.stop_requested()) {
        try {
            std::lock_guard<std::mutex> lock {device_mutex_};

            // Retrieve data from the device
            LOG(TRACE) << "Trying to receive data from device";
            const auto words = device_->getRawData();

            if(!words.empty()) {
                // Add data to message
                std::vector<std::uintptr_t> data;
                data.reserve(words.size());
                std::transform(words.begin(), words.end(), std::back_inserter(data), [](const auto& word) {
                    return static_cast<uintptr_t>(word);
                });
                msg.addFrame(std::move(data));

                // Query ADC if wanted:
                if(current_frames % adc_freq_ == 0) {
                    if(!adc_signal_.empty()) {
                        auto adc_value = device_->getADC(adc_signal_);
                        LOG(DEBUG) << "ADC reading: " << adc_signal_ << " =  " << std::to_string(adc_value);
                        msg.addTag(adc_signal_, adc_value);
                    }
                }

                // Increment frame counter
                current_frames++;
            }

            // If this message has all its frames, send it:
            if(msg.countFrames() == number_of_frames_) {
                trySendDataMessage(msg);

                // Create new data message
                msg = newDataMessage(number_of_frames_);
            }

        } catch(caribou::NoDataAvailable&) {
            continue;
        } catch(caribou::DataException& error) {
            // Retrieval failed, retry once more before aborting:
            LOG(WARNING) << error.what() << ", skipping data packet";
            continue;
        } catch(caribou::caribouException& error) {
            // Abort run by rethrowing exception
            throw SatelliteError(error.what());
        }
    }
}
