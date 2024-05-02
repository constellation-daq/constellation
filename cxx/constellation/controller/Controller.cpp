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

Controller::Controller(std::string_view controller_name) : logger_("CONTROLLER"), controller_name_(controller_name) {}

void Controller::registerSatellite(constellation::chirp::DiscoveredService service, bool depart, std::any /*unused*/) {
    // Check service.identifier == CONTROL
    if(service.identifier != chirp::ServiceIdentifier::CONTROL) {
        LOG(logger_, DEBUG) << "Wrong service " << magic_enum::enum_name(service.identifier) << " offered, ignoring";
        return;
    }

    // Add or drop, depending on message:
    const auto uri = "tcp://" + service.address.to_string() + ":" + std::to_string(service.port);
    if(depart) {
        for(auto it = satellite_connections_.begin(); it != satellite_connections_.end(); ++it) {
            if(it->second.host_id == service.host_id) {
                satellite_connections_.erase(it);
                LOG(logger_, INFO) << "Satellite at " << uri << " departed";
            }
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
