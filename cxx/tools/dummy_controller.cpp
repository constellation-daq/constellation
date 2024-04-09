/**
 * @file
 * @brief Dummy controller
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

#include <msgpack.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/logging/SinkManager.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/message/satellite_definitions.hpp"
#include "constellation/core/utils/casts.hpp"

using namespace constellation;
using namespace constellation::config;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::utils;
using namespace std::literals::chrono_literals;

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char* argv[]) {
    // Get address via cmdline
    if(argc != 2) {
        std::cout << "Invalid usage: dummy_controller CONSTELLATION_GROUP" << std::endl;
    }

    SinkManager::getInstance().setGlobalConsoleLevel(OFF);
    auto chirp_manager = chirp::Manager("255.255.255.255", "0.0.0.0", argv[1], "dummy_controller");
    chirp_manager.start();
    chirp_manager.sendRequest(chirp::ServiceIdentifier::CONTROL);

    // Wait a bit for service discovery
    auto discovered_services = chirp_manager.getDiscoveredServices(chirp::ServiceIdentifier::CONTROL);
    while(discovered_services.empty()) {
        std::cout << "Waiting for a satellite..." << std::endl;
        std::this_thread::sleep_for(100ms);
        discovered_services = chirp_manager.getDiscoveredServices(chirp::ServiceIdentifier::CONTROL);
    }
    auto uri = "tcp://" + discovered_services[0].address.to_string() + ":" + std::to_string(discovered_services[0].port);
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
            send_msg.addPayload(Configuration().assemble());
            std::cout << "Added empty configuration to message" << std::endl;
        } else if(command == "start") {
            const std::uint32_t run_nr = 1234U;
            msgpack::sbuffer sbuf {};
            msgpack::pack(sbuf, run_nr);
            send_msg.addPayload(std::make_shared<zmq::message_t>(sbuf.data(), sbuf.size()));
            std::cout << "Added run number " << run_nr << " to message" << std::endl;
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
                const auto dict = msgpack::unpack(to_char_ptr(recv_msg.getPayload()->data()), recv_msg.getPayload()->size())
                                      ->as<Dictionary>();
                if(!dict.empty()) {
                    std::cout << "Payload:";
                    for(const auto& [key, value] : dict) {
                        std::cout << "\n " << key << ": " << value.str();
                    }
                    std::cout << std::endl;
                }
            } catch(...) {
                std::cout << "Payload: <could not unpack payload>" << std::endl;
            }
        }
    }
}
