/**
 * @file
 * @brief Implementation of Satellite implementation
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "SatelliteImplementation.hpp"

#include <cctype>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include <magic_enum.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/logging/log.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/message/satellite_definitions.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/ports.hpp"
#include "constellation/core/utils/std23.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/fsm_definitions.hpp"

using namespace constellation;
using namespace constellation::message;
using namespace constellation::satellite;
using namespace constellation::utils;

SatelliteImplementation::SatelliteImplementation(std::shared_ptr<Satellite> satellite)
    : rep_(context_, zmq::socket_type::rep), port_(bind_ephemeral_port(rep_)), satellite_(std::move(satellite)),
      fsm_(satellite_), logger_("CSCP") {
    // Set receive timeout for socket
    rep_.set(zmq::sockopt::rcvtimeo, static_cast<int>(std::chrono::milliseconds(100).count()));
    // Announce service via CHIRP
    auto* chirp_manager = chirp::Manager::getDefaultInstance();
    if(chirp_manager != nullptr) {
        chirp_manager->registerService(chirp::CONTROL, port_);
    } else {
        LOG(logger_, WARNING) << "Failed to advertise command receiver on the network, satellite might not be discovered";
    }
    LOG(logger_, INFO) << "Starting to listen to commands on port " << port_;
}

SatelliteImplementation::~SatelliteImplementation() {
    main_thread_.request_stop();
    if(main_thread_.joinable()) {
        main_thread_.join();
    }
}

void SatelliteImplementation::start() {
    // jthread immediately starts on construction
    main_thread_ = std::jthread(std::bind_front(&SatelliteImplementation::main_loop, this));
}

void SatelliteImplementation::join() {
    if(main_thread_.joinable()) {
        main_thread_.join();
    }
}

std::optional<CSCP1Message> SatelliteImplementation::getNextCommand() {
    // Receive next message
    zmq::multipart_t recv_msg {};
    auto received = recv_msg.recv(rep_);

    // Return if timeout
    if(!received) {
        return std::nullopt;
    }

    // Try to disamble message
    auto message = CSCP1Message::disassemble(recv_msg);

    LOG(logger_, DEBUG) << "Received CSCP message of type " << to_string(message.getVerb().first) << " with verb \""
                        << message.getVerb().second << "\"" << (message.hasPayload() ? " and a payload"sv : ""sv) << " from "
                        << message.getHeader().getSender();

    return message;
}

void SatelliteImplementation::sendReply(std::pair<CSCP1Message::Type, std::string> reply_verb) {
    CSCP1Message({to_string(satellite_->getName())}, std::move(reply_verb)).assemble().send(rep_);
}

std::optional<std::pair<CSCP1Message::Type, std::string>>
SatelliteImplementation::handleGetCommand(std::string_view command) {
    auto return_verb = std::optional<std::pair<CSCP1Message::Type, std::string>>(std::nullopt);

    auto command_enum = magic_enum::enum_cast<GetCommand>(command);
    if(!command_enum.has_value()) {
        return return_verb;
    }

    using enum GetCommand;
    switch(command_enum.value()) {
    case get_name: {
        return_verb = {CSCP1Message::Type::SUCCESS, to_string(satellite_->getName())};
        break;
    }
    case get_commands: {
        // TODO(stephan.lachnit): return list of commands
        return_verb = {CSCP1Message::Type::NOTIMPLEMENTED, "Command get_commands is not implemented"};
        break;
    }
    case get_state: {
        return_verb = {CSCP1Message::Type::SUCCESS, to_string(fsm_.getState())};
        break;
    }
    case get_status: {
        return_verb = {CSCP1Message::Type::SUCCESS, to_string(satellite_->getStatus())};
        break;
    }
    case get_config: {
        return_verb = {CSCP1Message::Type::NOTIMPLEMENTED, "Command get_config is not implemented"};
        break;
    }
    default: std::unreachable();
    }

    return return_verb;
}

void SatelliteImplementation::main_loop(const std::stop_token& stop_token) {
    while(!stop_token.stop_requested()) {
        try {
            // Receive next command
            auto message_opt = getNextCommand();

            // Timeout, continue
            if(!message_opt.has_value()) {
                continue;
            }
            const auto& message = message_opt.value();

            // Ensure we have a REQUEST message
            if(message.getVerb().first != CSCP1Message::Type::REQUEST) {
                LOG(logger_, WARNING) << "Received message via CSCP that is not REQUEST type - ignoring";
                sendReply({CSCP1Message::Type::ERROR, "Can only handle CSCP messages with REQUEST type"});
                continue;
            }

            // Transform command to lower-case
            const std::string command_string = transform(message.getVerb().second, ::tolower);

            // Try to decode as transition
            auto transition_command = magic_enum::enum_cast<TransitionCommand>(command_string);
            if(transition_command.has_value()) {
                sendReply(fsm_.reactCommand(transition_command.value(), message.getPayload()));
                continue;
            }

            // Try to decode as other builtin (non-transition) commands
            auto get_command_reply = handleGetCommand(command_string);
            if(get_command_reply.has_value()) {
                sendReply(get_command_reply.value());
                continue;
            }

            // TODO(stephan.lachnit): Try to decode as user commands

            // Command is not known
            std::string unknown_command_reply = "Command \"";
            unknown_command_reply += command_string;
            unknown_command_reply += "\" is not known";
            LOG(logger_, WARNING) << "Received unknown command \"" << command_string << "\" - ignoring";
            sendReply({CSCP1Message::Type::UNKNOWN, std::move(unknown_command_reply)});

        } catch(const zmq::error_t& error) {
            LOG(logger_, CRITICAL) << "ZeroMQ error while trying to receive a message: " << error.what();
            LOG(logger_, CRITICAL) << "Stopping command receiver loop, no further commands can be received";
            break;
        } catch(const MessageDecodingError& error) {
            LOG(logger_, WARNING) << error.what();
            sendReply({CSCP1Message::Type::ERROR, error.what()});
        }
    }
}
