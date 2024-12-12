/**
 * @file
 * @brief Implementation of the CHIRP broadcast sender
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "BroadcastSend.hpp"

#include <algorithm>
#include <cstddef>
#include <set>
#include <span>
#include <string_view>
#include <utility>

#include <asio.hpp>

using namespace constellation::chirp;

BroadcastSend::BroadcastSend(const std::set<asio::ip::address_v4>& brd_addresses, asio::ip::port_type port) {
    for(const auto& brdaddr : brd_addresses) {
        const auto& endpoint = endpoints_.emplace_back(brdaddr, port);
        auto socket = asio::ip::udp::socket(io_context_, endpoint.protocol());

        // Set reusable address and broadcast socket options
        socket.set_option(asio::socket_base::reuse_address(true));
        socket.set_option(asio::socket_base::broadcast(true));
        // Set broadcast address for use in send() function
        socket.connect(endpoint);
        sockets_.push_back(std::move(socket));
    }
}

BroadcastSend::BroadcastSend(std::string_view brd_ip, asio::ip::port_type port)
    : BroadcastSend({asio::ip::make_address_v4(brd_ip)}, port) {}

void BroadcastSend::sendBroadcast(std::string_view message) {
    std::ranges::for_each(sockets_, [&](auto& sck) { sck.send(asio::buffer(message)); });
}

void BroadcastSend::sendBroadcast(std::span<const std::byte> message) {
    std::ranges::for_each(sockets_, [&](auto& sck) { sck.send(asio::const_buffer(message.data(), message.size_bytes())); });
}
