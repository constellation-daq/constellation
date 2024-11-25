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
#include <set>
#include <span>
#include <string_view>
#include <vector>

#include <asio/io_context.hpp>
#include <asio/ip/udp.hpp>

#include "constellation/build.hpp"

namespace constellation::chirp {

    /** Broadcast sender for outgoing broadcasts */
    class BroadcastSend {
    public:
        /**
         * Construct broadcast sender
         *
         * @param brd_addresses Set of broadcast addresses for outgoing broadcasts
         * @param port Port for outgoing broadcasts
         */
        CNSTLN_API BroadcastSend(const std::set<asio::ip::address_v4>& brd_addresses, asio::ip::port_type port);

        /**
         * Construct broadcast sender using a single human readable IP address
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
        std::vector<asio::ip::udp::endpoint> endpoints_;
        std::vector<asio::ip::udp::socket> sockets_;
    };

} // namespace constellation::chirp
