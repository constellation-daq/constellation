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
#include <functional>
#include <iostream>
#include <stop_token>
#include <string>

#include <magic_enum.hpp>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/heartbeat/HeartbeatSend.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation;
using namespace constellation::heartbeat;
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
    if(argc != 4) {
        std::cout << "Invalid usage: chp_sender CONSTELLATION_GROUP SENDER_NAME INTERVAL_MS" << std::endl;
        return 1;
    }

    auto chirp_manager = chirp::Manager("255.255.255.255", "0.0.0.0", argv[1], "chp_sender");
    chirp_manager.setAsDefaultInstance();
    chirp_manager.start();

    auto interval = std::chrono::milliseconds(std::stoi(argv[3]));
    HeartbeatSend sender {argv[2], interval};

    std::stop_source stop_token {};
    signal_handler_f = [&](int /*signal*/) -> void { stop_token.request_stop(); };

    // NOLINTBEGIN(cert-err33-c)
    std::signal(SIGTERM, &signal_hander);
    std::signal(SIGINT, &signal_hander);
    // NOLINTEND(cert-err33-c)

    auto state = State::NEW;
    while(!stop_token.stop_requested()) {
        std::cout << "-----------------------------------------" << std::endl;
        // Type
        std::string state_s {};
        std::cout << "State:    [" << to_string(state) << "] ";
        std::getline(std::cin, state_s);
        state = magic_enum::enum_cast<State>(state_s, magic_enum::case_insensitive).value_or(state);
        sender.updateState(state);
    }

    return 0;
}
