/**
 * @file
 * @brief Implementation of the CHIRP broadcast receiver
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "BroadcastRecv.hpp"

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

using namespace constellation::chirp;

constexpr std::size_t MESSAGE_BUFFER = 1024;

std::string BroadcastMessage::to_string() const {
    std::string ret;
    ret.resize(content.size());
    for(std::size_t n = 0; n < content.size(); ++n) {
        ret.at(n) = static_cast<char>(content[n]);
    }
    return ret;
}

BroadcastRecv::BroadcastRecv(const asio::ip::address_v4& any_address, asio::ip::port_type port)
    : endpoint_(any_address, port), socket_(io_context_, endpoint_.protocol()) {
    // Set reusable address socket option
    socket_.set_option(asio::socket_base::reuse_address(true));
    // Bind socket on receiving side
    socket_.bind(endpoint_);
}

BroadcastRecv::BroadcastRecv(std::string_view any_ip, asio::ip::port_type port)
    : BroadcastRecv(asio::ip::make_address_v4(any_ip), port) {}

BroadcastMessage BroadcastRecv::recvBroadcast() {
    BroadcastMessage message {};

    // Reserve some space for message
    message.content.resize(MESSAGE_BUFFER);

    // Receive content and length of message
    asio::ip::udp::endpoint sender_endpoint {};
    auto length = socket_.receive_from(asio::buffer(message.content), sender_endpoint);

    // Store IP address
    message.address = sender_endpoint.address().to_v4();

    // Resize content to actual message length
    message.content.resize(length);

    return message;
}

std::optional<BroadcastMessage> BroadcastRecv::asyncRecvBroadcast(std::chrono::steady_clock::duration timeout) {
    BroadcastMessage message {};
    message.content.resize(MESSAGE_BUFFER);
    asio::ip::udp::endpoint sender_endpoint {};

    // Receive as future
    auto length_future = socket_.async_receive_from(asio::buffer(message.content), sender_endpoint, asio::use_future);

    // Run IO context for timeout
    io_context_.restart();
    io_context_.run_for(timeout);

    // If IO context not stopped, then no message received
    if(!io_context_.stopped()) {
        // Cancel async operations
        socket_.cancel();
        return std::nullopt;
    }

    message.address = sender_endpoint.address().to_v4();
    message.content.resize(length_future.get());
    return message;
}
