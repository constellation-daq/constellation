/**
 * @file
 * @brief CHIRP socket
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <asio/io_context.hpp>
#include <asio/ip/address_v4.hpp>
#include <asio/ip/udp.hpp>

namespace constellation::chirp {

    /** CHIRP multicast address */
    const auto CHIRP_MULTICAST_ADDRESS = asio::ip::address_v4({239, 192, 49, 192});

    /** CHIRP port */
    constexpr asio::ip::port_type CHIRP_PORT = 49192;

    /** CHIRP UDP endpoint for asio */
    const auto CHIRP_ENDPOINT = asio::ip::udp::endpoint(CHIRP_MULTICAST_ADDRESS, CHIRP_PORT);

    /** Multicast TTL (network hops) for CHIRP socket */
    constexpr int CHIRP_MULTICAST_TTL = 8;

    /** Message buffer for CHIRP (max mesasge length) */
    constexpr std::size_t CHIRP_BUFFER_SIZE = 1024;

    class CHIRPSocket {
    public:
        /**
         * @brief Construct a new socket bound to CHIRP
         *
         * @param interface IP address of the network interface to use
         */
        CHIRPSocket(asio::ip::address_v4 interface = asio::ip::address_v4::any());

        /**
         * @brief Try to receive a message until a timeout is reached
         *
         * @param timeout Timeout to wait before returning an empty `std::optional`
         * @return Pair of message in bytes and sending endpoint if timeout not reached
         */
        std::optional<std::pair<std::vector<std::byte>, asio::ip::udp::endpoint>>
        recv(std::chrono::steady_clock::duration timeout);

        /**
         * @brief Send a message
         *
         * @param bytes View of the message in bytes
         */
        void send(std::span<const std::byte> bytes);

        /**
         * @brief Get the interface address
         */
        const asio::ip::address_v4& getInterface() const { return interface_; };

        /**
         * @brief Set the interface address
         */
        void setInterface(asio::ip::address_v4 interface);

    private:
        asio::io_context io_context_;
        asio::ip::udp::socket socket_;
        asio::ip::address_v4 interface_;
    };

} // namespace constellation::chirp
