/**
 * @file
 * @brief Implementation of CHIRP socket
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "CHIRPSocket.hpp"

#include <chrono>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <asio.hpp>

using namespace constellation::chirp;

CHIRPSocket::CHIRPSocket(asio::ip::address_v4 interface) : socket_(io_context_), interface_(std::move(interface)) {
    // Open socket to set UDP as protocol
    socket_.open(CHIRP_ENDPOINT.protocol());

    // Set SO_REUSEADDR to ensure socket can be bound by other programs
    socket_.set_option(asio::ip::udp::socket::reuse_address(true));

    // Enable loopback interface
    socket_.set_option(asio::ip::multicast::enable_loopback(true));

    // Set multicast TTL (network hops)
    socket_.set_option(asio::ip::multicast::hops(CHIRP_MULTICAST_TTL));

    // Set network interface
    socket_.set_option(asio::ip::multicast::outbound_interface(interface_));

    // Bind socket to multicast endpoint
    socket_.bind(CHIRP_ENDPOINT);

    // Join multicast group
    socket_.set_option(asio::ip::multicast::join_group(CHIRP_MULTICAST_ADDRESS));
}

std::optional<std::pair<std::vector<std::byte>, asio::ip::udp::endpoint>>
CHIRPSocket::recv(std::chrono::steady_clock::duration timeout) {
    std::vector<std::byte> buffer {};
    buffer.resize(CHIRP_BUFFER_SIZE);
    asio::ip::udp::endpoint sender_endpoint {};

    // Receive message as future
    auto message_length_future = socket_.async_receive_from(asio::buffer(buffer), sender_endpoint, asio::use_future);

    // Run IO context for timeout
    io_context_.restart();
    io_context_.run_for(timeout);

    // If IO context not stopped, then no message received
    if(!io_context_.stopped()) {
        // Cancel async operations
        socket_.cancel();
        return std::nullopt;
    }

    // Resize buffer to actual message length
    const auto message_length = message_length_future.get();
    buffer.resize(message_length);

    return std::make_pair(std::move(buffer), std::move(sender_endpoint));
}

void CHIRPSocket::send(std::span<const std::byte> bytes) {
    socket_.send_to(asio::const_buffer(bytes.data(), bytes.size()), CHIRP_ENDPOINT);
}

void CHIRPSocket::setInterface(asio::ip::address_v4 interface) {
    interface_ = std::move(interface);
    socket_.set_option(asio::ip::multicast::outbound_interface(interface_));
}
