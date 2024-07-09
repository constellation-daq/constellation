/**
 * @file
 * @brief CHIRP broadcast sender
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstddef>
#include <span>
#include <string_view>

#include <asio.hpp>

#include "constellation/build.hpp"

namespace constellation::chirp {

    /** Broadcast sender for outgoing broadcasts */
    class BroadcastSend {
    public:
        /**
         * Construct broadcast sender
         *
         * @param brd_address Broadcast address for outgoing broadcasts (e.g. `asio::ip::address_v4::broadcast()`)
         * @param port Port for outgoing broadcasts
         */
        CNSTLN_API BroadcastSend(const asio::ip::address_v4& brd_address, asio::ip::port_type port);

        /**
         * Construct broadcast sender using human readable IP address
         *
         * @param brd_ip String containing the broadcast IP for outgoing broadcasts (e.g. `255.255.255.255`)
         * @param port Port for outgoing broadcasts
         */
        CNSTLN_API BroadcastSend(std::string_view brd_ip, asio::ip::port_type port);

        /**
         * Send broadcast message from string
         *
         * @param message String with broadcast message
         */
        CNSTLN_API void sendBroadcast(std::string_view message);

        /**
         * Send broadcast message
         *
         * @param message View of message in bytes
         */
        CNSTLN_API void sendBroadcast(std::span<const std::byte> message);

    private:
        asio::io_context io_context_;
        asio::ip::udp::endpoint endpoint_;
        asio::ip::udp::socket socket_;
    };

} // namespace constellation::chirp
