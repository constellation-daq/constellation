/**
 * @file
 * @brief Dummy controller
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <iostream>
#include <span>
#include <string>
#include <utility>

#include <msgpack.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/logging/SinkManager.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/message/satellite_definitions.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation;
using namespace constellation::config;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::utils;
using namespace std::literals::chrono_literals;
using namespace std::literals::string_literals;

void cli_loop(std::span<char*> args) {
    // Get group via cmdline
    std::cout << "Usage: chp_receiver CONSTELLATION_GROUP" << std::endl;

    auto group = "constellation"s;
    if(args.size() >= 2) {
        group = args[1];
    }
    std::cout << "Using constellation group " << std::quoted(group) << std::endl;

    SinkManager::getInstance().setGlobalConsoleLevel(OFF);
    auto chirp_manager = chirp::Manager("255.255.255.255", "0.0.0.0", group, "dummy_controller");
    chirp_manager.start();
    chirp_manager.sendRequest(chirp::ServiceIdentifier::CONTROL);

    // Wait a bit for service discovery
    auto discovered_services = chirp_manager.getDiscoveredServices(chirp::ServiceIdentifier::CONTROL);
    while(discovered_services.empty()) {
        std::cout << "Waiting for a satellite..." << std::endl;
        std::this_thread::sleep_for(100ms);
        discovered_services = chirp_manager.getDiscoveredServices(chirp::ServiceIdentifier::CONTROL);
    }
    auto uri = "tcp://" + discovered_services[0].address.to_string() + ":" + to_string(discovered_services[0].port);
    std::cout << "Connecting to " << uri << std::endl;

    zmq::context_t context {};
    zmq::socket_t req {context, zmq::socket_type::req};

    req.connect(uri);

    while(true) {
        std::string command;
        std::cout << "Send command: ";
        std::getline(std::cin, command);

        // Send command
        auto send_msg = CSCP1Message({"dummy_controller"}, {CSCP1Message::Type::REQUEST, command});
        if(command == "initialize" || command == "reconfigure") {
            send_msg.addPayload(Dictionary().assemble());
            std::cout << "Added empty configuration to message" << std::endl;
        } else if(command == "start") {
            const std::string run_identifier = "1234";
            msgpack::sbuffer sbuf {};
            msgpack::pack(sbuf, run_identifier);
            send_msg.addPayload(std::move(sbuf));
            std::cout << "Added run identifier \"" << run_identifier << "\" to message" << std::endl;
        }
        send_msg.assemble().send(req);

        // Receive reply
        zmq::multipart_t recv_zmq_msg {};
        recv_zmq_msg.recv(req);
        auto recv_msg = CSCP1Message::disassemble(recv_zmq_msg);

        // Print message
        std::cout << recv_msg.getHeader().to_string() << "\n"
                  << "Verb: " << to_string(recv_msg.getVerb().first) << " : " << recv_msg.getVerb().second << std::endl;

        // Print payload if dict
        if(recv_msg.hasPayload()) {
            try {
                const auto dict = Dictionary::disassemble(recv_msg.getPayload());
                if(!dict.empty()) {
                    std::cout << "Payload:" << dict.to_string() << std::endl;
                }
            } catch(...) {
                std::cout << "Payload: <could not unpack payload>" << std::endl;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    try {
        cli_loop(std::span(argv, argc));
    } catch(...) {
        return 1;
    }
    return 0;
}
