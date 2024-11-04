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
#include <memory>
#include <string_view>

#include <zmq.hpp>

#include "constellation/build.hpp"

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
    CNSTLN_API inline Port bind_ephemeral_port(zmq::socket_t& socket) {
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
     * @brief Return the global ZeroMQ context
     *
     * @note Since the global ZeroMQ context is static, static classes need to store an instance of the shared pointer.
     *
     * @return Shared pointer to the global ZeroMQ context
     */
    CNSTLN_API inline std::shared_ptr<zmq::context_t>& global_zmq_context() {
        static auto context = std::make_shared<zmq::context_t>();
        // Switch off blocky behavior of context - corresponds to setting linger = 0 for all sockets
        context->set(zmq::ctxopt::blocky, 0);
        return context;
    }

} // namespace constellation::utils
