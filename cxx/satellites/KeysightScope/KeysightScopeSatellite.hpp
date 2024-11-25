/**
 * @file
 * @brief Prototype satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <string>
#include <string_view>

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/satellite/TransmitterSatellite.hpp"

class KeysightScopeSatellite final : public constellation::satellite::TransmitterSatellite {
public:
    KeysightScopeSatellite(std::string_view type, std::string_view name);

    void initializing(constellation::config::Configuration& config) final;

private:
    void send(std::string_view command);
    std::string recv();
    std::string send_recv(std::string_view command);

private:
    asio::io_context io_context_;
    asio::ip::tcp::socket socket_;
};
