/**
 * @file
 * @brief CHP heartbeat sender
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <csignal>
#include <iostream>
#include <stop_token>
#include <string>
#include <thread>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/heartbeat/HeartbeatRecv.hpp"
#include "constellation/core/logging/log.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation;
using namespace constellation::heartbeat;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::utils;
using namespace std::literals::chrono_literals;

// Use global std::function to work around C linkage
std::function<void(int)> signal_handler_f {}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

extern "C" void signal_hander(int signal) {
    signal_handler_f(signal);
}

int main(int argc, char* argv[]) {
    // Get address via cmdline
    if(argc != 2) {
        std::cout << "Invalid usage: chp_receiver CONSTELLATION_GROUP" << std::endl;
        return 1;
    }

    auto chirp_manager = chirp::Manager("255.255.255.255", "0.0.0.0", argv[1], "chp_receiver");
    chirp_manager.setAsDefaultInstance();
    chirp_manager.start();

    Logger logger {"chp_receiver"};

    const HeartbeatRecv receiver {[&](const CHP1Message& msg) {
        LOG(logger, DEBUG) << msg.getSender() << " reports state " << to_string(msg.getState()) << ", next message in "
                           << to_string(msg.getInterval());
    }};

    std::stop_source stop_token;
    signal_handler_f = [&](int /*signal*/) -> void { stop_token.request_stop(); };

    // NOLINTBEGIN(cert-err33-c)
    std::signal(SIGTERM, &signal_hander);
    std::signal(SIGINT, &signal_hander);
    // NOLINTEND(cert-err33-c)

    while(!stop_token.stop_requested()) {
        std::this_thread::sleep_for(100ms);
    }

    return 0;
}
