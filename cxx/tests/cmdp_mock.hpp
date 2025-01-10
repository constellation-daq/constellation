/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <string>
#include <string_view>
#include <utility>

#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/log/Level.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/networking/Port.hpp"
#include "constellation/core/networking/zmq_helpers.hpp"

class CMDPSender {
public:
    CMDPSender(std::string name)
        : name_(std::move(name)), pub_socket_(*constellation::networking::global_zmq_context(), zmq::socket_type::xpub),
          port_(constellation::networking::bind_ephemeral_port(pub_socket_)) {}

    constellation::networking::Port getPort() const { return port_; }

    std::string_view getName() const { return name_; }

    void sendLogMessage(constellation::log::Level level, std::string topic, std::string message) {
        auto msg = constellation::message::CMDP1LogMessage(level, std::move(topic), {"CMDPSender.s1"}, std::move(message));
        msg.assemble().send(pub_socket_);
    }

    void sendRaw(zmq::multipart_t& msg) { msg.send(pub_socket_); }

    zmq::multipart_t recv() {
        zmq::multipart_t recv_msg {};
        recv_msg.recv(pub_socket_);
        return recv_msg;
    }

    bool canRecv() {
        zmq::message_t msg {};
        pub_socket_.set(zmq::sockopt::rcvtimeo, 200);
        auto recv_res = pub_socket_.recv(msg);
        pub_socket_.set(zmq::sockopt::rcvtimeo, -1);
        return recv_res.has_value();
    }

private:
    std::string name_;
    zmq::socket_t pub_socket_;
    constellation::networking::Port port_;
};
