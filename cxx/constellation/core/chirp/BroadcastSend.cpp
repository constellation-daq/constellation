/**
 * @file
 * @brief Implementation of the CHIRP broadcast sender
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "BroadcastSend.hpp"

#include <cstddef>
#include <span>
#include <string_view>

using namespace constellation::chirp;

BroadcastSend::BroadcastSend(const asio::ip::address_v4& brd_address, asio::ip::port_type port)
    : endpoint_(brd_address, port), socket_(io_context_, endpoint_.protocol()) {
    // Set reusable address and broadcast socket options
    socket_.set_option(asio::socket_base::reuse_address(true));
    socket_.set_option(asio::socket_base::broadcast(true));
    // Set broadcast address for use in send() function
    socket_.connect(endpoint_);
}

BroadcastSend::BroadcastSend(std::string_view brd_ip, asio::ip::port_type port)
    : BroadcastSend(asio::ip::make_address_v4(brd_ip), port) {}

void BroadcastSend::sendBroadcast(std::string_view message) {
    socket_.send(asio::buffer(message));
}

void BroadcastSend::sendBroadcast(std::span<const std::byte> message) {
    socket_.send(asio::const_buffer(message.data(), message.size_bytes()));
}
