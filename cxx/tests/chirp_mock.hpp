/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <algorithm>
#include <chrono>
#include <memory>
#include <string_view>
#include <thread>
#include <tuple>

#include "constellation/core/chirp/BroadcastSend.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/networking/Port.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"

class SingletonChirpManager {
public:
    SingletonChirpManager()
        : manager_(std::make_shared<constellation::chirp::Manager>("0.0.0.0", "0.0.0.0", "edda", "chirp_manager")) {
        manager_->setAsDefaultInstance();
        manager_->start();
    }

    std::shared_ptr<constellation::chirp::Manager> getManager() { return manager_; }

private:
    std::shared_ptr<constellation::chirp::Manager> manager_;
};

inline std::shared_ptr<constellation::chirp::Manager> create_chirp_manager() {
    static SingletonChirpManager chirpmanager;
    return chirpmanager.getManager();
}

inline void chirp_mock_service(std::string_view name,
                               constellation::protocol::CHIRP::ServiceIdentifier service,
                               constellation::networking::Port port,
                               bool offer = true) {
    // Hack: add fake satellite to chirp to find satellite (cannot find from same manager)
    using namespace constellation::chirp;
    using namespace constellation::protocol::CHIRP;
    BroadcastSend chirp_sender {"0.0.0.0", PORT};
    const auto msgtype = offer ? MessageType::OFFER : MessageType::DEPART;
    const auto chirp_msg = constellation::message::CHIRPMessage(msgtype, "edda", name, service, port);
    chirp_sender.sendBroadcast(chirp_msg.assemble());
    // Wait until broadcast is received
    auto* manager = Manager::getDefaultInstance();
    while(std::ranges::count(manager->getDiscoveredServices(),
                             std::make_tuple(constellation::message::MD5Hash(name), service, port),
                             [](const auto& discovered_service) {
                                 return std::make_tuple(
                                     discovered_service.host_id, discovered_service.identifier, discovered_service.port);
                             }) == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
