/**
 * @file
 * @brief CHIRP broadcast receiver
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <asio.hpp>

#include "constellation/build.hpp"

namespace constellation::chirp {

    /** Incoming broadcast message */
    struct BroadcastMessage {
        /** Content of the broadcast message in bytes */
        std::vector<std::byte> content;

        /** Address from which the broadcast message was received */
        asio::ip::address_v4 address;

        /** Convert the content of the broadcast message to a string */
        CNSTLN_API std::string to_string() const;
    };

    /** Broadcast receiver for incoming CHIRP broadcasts on `CHIRP_PORT` */
    class BroadcastRecv {
    public:
        /**
         * Construct broadcast receiver
         *
         * @param any_address Address for incoming broadcasts (e.g. `asio::ip::address_v4::any()`)
         * @param port Port for outgoing broadcasts
         */
        CNSTLN_API BroadcastRecv(const asio::ip::address_v4& any_address, asio::ip::port_type port);

        /**
         * Construct broadcast receiver using human readable IP address
         *
         * @param any_ip String containing the IP for incoming broadcasts (e.g. `0.0.0.0`)
         * @param port Port for outgoing broadcasts
         */
        CNSTLN_API BroadcastRecv(std::string_view any_ip, asio::ip::port_type port);

        /**
         * Receive broadcast message (blocking)
         *
         * @return Received broadcast message
         */
        CNSTLN_API BroadcastMessage recvBroadcast();

        /**
         * Receive broadcast message (asynchronously)
         *
         * @param timeout Duration for which to block function call
         * @return Broadcast message if received
         */
        CNSTLN_API std::optional<BroadcastMessage> asyncRecvBroadcast(std::chrono::steady_clock::duration timeout);

    private:
        asio::io_context io_context_;
        asio::ip::udp::endpoint endpoint_;
        asio::ip::udp::socket socket_;
    };

} // namespace constellation::chirp
