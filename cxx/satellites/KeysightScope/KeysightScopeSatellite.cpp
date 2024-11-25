/**
 * @file
 * @brief Implementation of the Sputnik prototype satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "KeysightScopeSatellite.hpp"

#include <cstdint>
#include <string>
#include <string_view>

#include <asio.hpp>
#include <asio/buffer.hpp>
#include <asio/ip/address_v4.hpp>
#include <asio/system_error.hpp>
#include <asio/use_future.hpp>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/exceptions.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/networking/asio_helpers.hpp"
#include "constellation/satellite/TransmitterSatellite.hpp"

using namespace constellation::satellite;
using namespace constellation::config;
using namespace constellation::networking;

KeysightScopeSatellite::KeysightScopeSatellite(std::string_view type, std::string_view name)
    : TransmitterSatellite(type, name), socket_(io_context_) {}

void KeysightScopeSatellite::initializing(Configuration& config) {
    const auto address_str = config.get<std::string>("address");
    const auto port = config.get<std::uint16_t>("port", 5025);

    asio::ip::address_v4 address;
    try {
        address = asio::ip::make_address_v4(address_str);
    } catch(const asio::system_error& error) {
        throw InvalidValueError(config, "address", error.what());
    }

    LOG(INFO) << "Connecting to " << to_uri(address, port);
    asio::ip::tcp::endpoint endpoint = {address, port};
    socket_.connect(endpoint);
    // socket_.bind(endpoint); // TODO(stephan.lachnit): rethrow

    // Identify device
    const auto idn = send_recv("*IDN?");
    LOG(INFO) << "Connected to " << idn;
}

constexpr std::chrono::seconds TIMEOUT {1};
constexpr std::size_t BUFFER_SIZE {2048};

void KeysightScopeSatellite::send(std::string_view command) {
    // Create future for message
    auto length_future = socket_.async_send(asio::buffer(command), asio::use_future);

    // Run IO context for timeout
    io_context_.restart();
    io_context_.run_for(TIMEOUT);

    // If IO context not stopped, then no message received
    if(!io_context_.stopped()) {
        // Cancel async operations
        socket_.cancel();
        return; // TODO(stephan.lachnit): throw
    }

    length_future.get();
}

std::string KeysightScopeSatellite::recv() {
    std::string buffer;
    buffer.resize(BUFFER_SIZE);

    // Create future for message
    auto length_future = socket_.async_receive(asio::buffer(buffer), asio::use_future);

    // Run IO context for timeout
    io_context_.restart();
    io_context_.run_for(TIMEOUT);

    // If IO context not stopped, then no message received
    if(!io_context_.stopped()) {
        // Cancel async operations
        socket_.cancel();
        return buffer; // TODO(stephan.lachnit): throw
    }

    // Check length
    const auto length = length_future.get();
    if(length > BUFFER_SIZE) {
        // TODO(stephan.lachnit): throw
    }

    buffer.resize(length);
    return buffer;
}

std::string KeysightScopeSatellite::send_recv(std::string_view command) {
    send(command);
    return recv();
}
