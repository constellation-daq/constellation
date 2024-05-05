/**
 * @file
 * @brief Implementation of Controller class
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Controller.hpp"

#include <string>
#include <string_view>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/logging/log.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/fsm_definitions.hpp"

using namespace constellation::controller;
using namespace constellation::message;
using namespace constellation::utils;

Controller::Controller(std::string_view controller_name) : logger_("CONTROLLER"), controller_name_(controller_name) {
    LOG(logger_, DEBUG) << "Registering controller callback";
    chirp::Manager::getDefaultInstance()->registerDiscoverCallback(&Controller::callback, chirp::CONTROL, this);
}

void Controller::callback(chirp::DiscoveredService service, bool depart, std::any user_data) {
    auto* instance = std::any_cast<Controller*>(user_data);
    instance->callback_impl(std::move(service), depart);
}

void Controller::callback_impl(const constellation::chirp::DiscoveredService& service, bool depart) {

    // Add or drop, depending on message:
    const auto uri = service.to_uri();
    if(depart) {
        const auto it = std::find_if(satellite_connections_.begin(), satellite_connections_.end(), [&](const auto& sat) {
            return sat.second.host_id == service.host_id;
        });
        if(it != satellite_connections_.end()) {
            satellite_connections_.erase(it);
            LOG(logger_, INFO) << "Satellite at " << uri << " departed";
        }
    } else {
        // New satellite connection
        Connection conn = {{context_, zmq::socket_type::req}, service.host_id};
        conn.req.connect(uri);

        // Obtain canonical name:
        auto send_msg = CSCP1Message({controller_name_}, {CSCP1Message::Type::REQUEST, "get_name"});
        send_msg.assemble().send(conn.req);
        zmq::multipart_t recv_zmq_msg {};
        recv_zmq_msg.recv(conn.req);
        const auto recv_msg = CSCP1Message::disassemble(recv_zmq_msg);
        const auto name = recv_msg.getVerb().second;

        const auto [it, success] = satellite_connections_.emplace(name, std::move(conn));

        if(!success) {
            LOG(logger_, DEBUG) << "Not adding remote satellite at " << uri << ", was already registered";
        } else {
            LOG(logger_, INFO) << "Registered remote satellite at " << uri;
        }
    }
}

CSCP1Message Controller::send_receive(Connection& conn, CSCP1Message& cmd) {
    cmd.assemble().send(conn.req);
    zmq::multipart_t recv_zmq_msg {};
    recv_zmq_msg.recv(conn.req);
    return CSCP1Message::disassemble(recv_zmq_msg);
}

CSCP1Message Controller::sendCommand(std::string_view satellite_name, CSCP1Message& cmd) {
    const auto sat = satellite_connections_.find(satellite_name);
    if(sat == satellite_connections_.end()) {
        throw;
    }

    return send_receive(sat->second, cmd);
}

std::map<std::string, CSCP1Message> Controller::sendCommand(CSCP1Message& cmd) {
    std::map<std::string, CSCP1Message> replies;
    for(auto& sat : satellite_connections_) {
        replies.emplace(sat.first, send_receive(sat.second, cmd));
    }
    return replies;
}
