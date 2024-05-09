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

using namespace constellation::config;
using namespace constellation::controller;
using namespace constellation::message;
using namespace constellation::satellite;
using namespace constellation::utils;

Controller::Controller(std::string_view controller_name) : logger_("CONTROLLER"), controller_name_(controller_name) {
    LOG(logger_, DEBUG) << "Registering controller callback";
    auto* chirp_manager = chirp::Manager::getDefaultInstance();
    if(chirp_manager != nullptr) {
        chirp_manager->registerDiscoverCallback(&Controller::callback, chirp::CONTROL, this);
        chirp_manager->sendRequest(chirp::CONTROL);
    }
}

Controller::~Controller() {
    const std::lock_guard connection_lock {connection_mutex_};

    for(auto& conn : connections_) {
        conn.second.req.close();
    }
    connections_.clear();
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
void Controller::callback(chirp::DiscoveredService service, bool depart, std::any user_data) {
    auto* instance = std::any_cast<Controller*>(user_data);
    instance->callback_impl(service, depart);
}

void Controller::callback_impl(const constellation::chirp::DiscoveredService& service, bool depart) {

    const std::lock_guard connection_lock {connection_mutex_};

    // Add or drop, depending on message:
    const auto uri = service.to_uri();
    if(depart) {
        const auto it = std::find_if(connections_.begin(), connections_.end(), [&](const auto& sat) {
            return sat.second.host_id == service.host_id;
        });
        if(it != connections_.end()) {
            it->second.req.close();
            connections_.erase(it);
            LOG(logger_, INFO) << "Satellite at " << uri << " departed";
        }
    } else {
        // New satellite connection
        Connection conn = {{context_, zmq::socket_type::req}, service.host_id};
        conn.req.connect(uri);

        // Obtain canonical name:
        auto send_msg = CSCP1Message({controller_name_}, {CSCP1Message::Type::REQUEST, "get_name"});
        const auto recv_msg = send_receive(conn, send_msg);
        const auto name = recv_msg.getVerb().second;

        const auto [it, success] = connections_.emplace(name, std::move(conn));

        if(!success) {
            LOG(logger_, DEBUG) << "Not adding remote satellite at " << uri << ", was already registered";
        } else {
            LOG(logger_, INFO) << "Registered remote satellite at " << uri;
        }
    }
}

bool Controller::isInState(State state) {
    const std::lock_guard connection_lock {connection_mutex_};

    if(std::ranges::all_of(
           connections_.cbegin(), connections_.cend(), [state](const auto& conn) { return conn.second.state == state; })) {
        return true;
    }

    return false;
}

State Controller::getLowestState() {
    const std::lock_guard connection_lock {connection_mutex_};

    if(connections_.empty()) {
        return State::NEW;
    }

    State state {State::ERROR};
    for(const auto& conn : connections_) {
        if(conn.second.state < state) {
            state = conn.second.state;
        }
    }
    return state;
}

CSCP1Message Controller::send_receive(Connection& conn, CSCP1Message& cmd) {
    // Keep payload, we might send multiple command messages:
    cmd.assemble(true).send(conn.req);
    zmq::multipart_t recv_zmq_msg {};
    recv_zmq_msg.recv(conn.req);
    return CSCP1Message::disassemble(recv_zmq_msg);
}

CSCP1Message Controller::sendCommand(std::string_view satellite_name, CSCP1Message& cmd) {
    const std::lock_guard connection_lock {connection_mutex_};

    const auto sat = connections_.find(satellite_name);
    if(sat == connections_.end()) {
        return CSCP1Message({controller_name_}, {CSCP1Message::Type::ERROR, "Target satellite is unknown to controller"});
    }

    return send_receive(sat->second, cmd);
}

std::map<std::string, CSCP1Message> Controller::sendCommand(CSCP1Message& cmd) {

    const std::lock_guard connection_lock {connection_mutex_};
    std::map<std::string, CSCP1Message> replies;
    for(auto& sat : connections_) {
        replies.emplace(sat.first, send_receive(sat.second, cmd));
    }
    return replies;
}

std::map<std::string, CSCP1Message> Controller::sendCommand(const std::string& verb, const CommandPayload& payload) {
    auto send_msg = CSCP1Message({controller_name_}, {CSCP1Message::Type::REQUEST, {verb}});
    if(std::holds_alternative<Dictionary>(payload)) {
        send_msg.addPayload(std::get<Dictionary>(payload).assemble());
    } else if(std::holds_alternative<List>(payload)) {
        send_msg.addPayload(std::get<List>(payload).assemble());
    } else if(std::holds_alternative<std::uint32_t>(payload)) {
        msgpack::sbuffer sbuf {};
        msgpack::pack(sbuf, std::get<std::uint32_t>(payload));
        send_msg.addPayload(std::move(sbuf));
    }
    return sendCommand(send_msg);
}

CSCP1Message Controller::sendCommand(std::string_view satellite_name,
                                     const std::string& verb,
                                     const CommandPayload& payload) {
    auto send_msg = CSCP1Message({controller_name_}, {CSCP1Message::Type::REQUEST, {verb}});
    if(std::holds_alternative<Dictionary>(payload)) {
        send_msg.addPayload(std::get<Dictionary>(payload).assemble());
    } else if(std::holds_alternative<List>(payload)) {
        send_msg.addPayload(std::get<List>(payload).assemble());
    } else if(std::holds_alternative<std::uint32_t>(payload)) {
        msgpack::sbuffer sbuf {};
        msgpack::pack(sbuf, std::get<std::uint32_t>(payload));
        send_msg.addPayload(std::move(sbuf));
    }
    return sendCommand(satellite_name, send_msg);
}
