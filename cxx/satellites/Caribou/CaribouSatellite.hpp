/**
 * @file
 * @brief Caribou Satellite definition
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>

// Caribou Peary includes
#include <peary/device/Device.hpp>
#include <peary/device/DeviceManager.hpp>

#include "constellation/core/logging/Level.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::config;
using namespace constellation::satellite;

class CaribouLogger final : public std::stringbuf {
public:
    CaribouLogger() : logger_("Caribou") {}
    int sync() final;
    std::ostream& stream();
    static constellation::log::Level getLogLevel(char short_log_format_char);

private:
    constellation::log::Logger logger_;
};

class CaribouSatellite : public Satellite {
public:
    CaribouSatellite(std::string_view type, std::string_view name);
    ~CaribouSatellite();

public:
    void initializing(Configuration& config) override;
    void launching() override;
    void landing() override;
    void reconfiguring(const Configuration& partial_config) override;
    void starting(std::string_view run_identifier) override;
    void stopping() override;
    void running(const std::stop_token& stop_token) override;

private:
    std::string device_class_;
    Configuration config_;

    std::shared_ptr<caribou::DeviceManager> manager_;
    caribou::Device* device_ {nullptr};
    caribou::Device* secondary_device_ {nullptr};

    CaribouLogger caribou_logger_;

    std::mutex device_mutex_;

    std::uint64_t frame_nr_;

    std::string adc_signal_;
    uint64_t adc_freq_;
};
