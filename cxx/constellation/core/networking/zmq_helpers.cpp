/**
 * @file
 * @brief Implementation of ZeroMQ helper functions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "zmq_helpers.hpp"

#include <charconv>
#include <memory>

#include <zmq.hpp>

using namespace constellation::networking;

Port constellation::networking::bind_ephemeral_port(zmq::socket_t& socket) {
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

std::shared_ptr<zmq::context_t>& constellation::networking::global_zmq_context() {
    static auto context = std::make_shared<zmq::context_t>();
    // Switch off blocky behavior of context - corresponds to setting linger = 0 for all sockets
    context->set(zmq::ctxopt::blocky, 0);
    return context;
}
