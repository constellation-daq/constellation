/**
 * @file
 * @brief Helpers for ZeroMQ
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

#include <asio/ip/address_v4.hpp>
#include <zmq.hpp>

#include "constellation/core/utils/string.hpp"

namespace constellation::utils {

    /**
     * @brief Port number for a network connection
     *
     * Note that most ports in Constellation are ephemeral ports, meaning that the port numbers are allocated dynamically.
     * See also https://en.wikipedia.org/wiki/Ephemeral_port.
     */
    using Port = std::uint16_t;

    /**
     * @brief Bind ZeroMQ socket to wildcard address with ephemeral port
     *
     * See also https://libzmq.readthedocs.io/en/latest/zmq_tcp.html.
     *
     * @param socket Reference to socket which should be bound
     * @return Ephemeral port assigned by operating system
     */
    static inline Port bind_ephemeral_port(zmq::socket_t& socket) {
        Port port {};

        // Bind to wildcard address and port to let operating system assign an ephemeral port
        socket.bind("tcp://*:*");

        // Get address with ephemeral port via last endpoint
        const auto endpoint = socket.get(zmq::sockopt::last_endpoint);

        // Note: endpoint is always "tcp://0.0.0.0:XXXXX", thus port number starts at character 14
        const auto port_substr = std::string_view(endpoint).substr(14);
        std::from_chars(port_substr.cbegin(), port_substr.cend(), port);

        return port;
    }

    /**
     * @brief Converts an IP address to a human-readble IP
     */
    static inline std::string address_to_ip(const asio::ip::address_v4& address) {
        return range_to_string(address.to_bytes(), ".");
    }

    /**
     * @brief Converts an endpoint (IP address and port) to a URI with given protocol
     */
    static inline std::string endpoint_to_uri(const std::string& protocol,
                                              const asio::ip::address_v4& address,
                                              utils::Port port) {
        return protocol + "://" + address_to_ip(address) + ":" + to_string(port);
    }

} // namespace constellation::utils
