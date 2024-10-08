/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <memory>
#include <string_view>
#include <thread>

#include "constellation/core/chirp/BroadcastSend.hpp"
#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/utils/networking.hpp"

inline std::shared_ptr<constellation::chirp::Manager> create_chirp_manager() {
    auto manager = std::make_shared<constellation::chirp::Manager>("0.0.0.0", "0.0.0.0", "edda", "chirp_manager");
    manager->setAsDefaultInstance();
    manager->start();
    return manager;
}

inline void chirp_mock_service(std::string_view name,
                               constellation::chirp::ServiceIdentifier service,
                               constellation::utils::Port port,
                               bool offer = true) {
    // Hack: add fake satellite to chirp to find satellite (cannot find from same manager)
    using namespace constellation::chirp;
    BroadcastSend chirp_sender {"0.0.0.0", CHIRP_PORT};
    const auto msgtype = offer ? MessageType::OFFER : MessageType::DEPART;
    const auto chirp_msg = constellation::message::CHIRPMessage(msgtype, "edda", name, service, port);
    chirp_sender.sendBroadcast(chirp_msg.assemble());
    // Sleep a bit to handle chirp broadcast
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}
