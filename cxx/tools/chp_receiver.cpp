/**
 * @file
 * @brief CHP heartbeat sender
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono> // IWYU pragma: keep
#include <csignal>
#include <functional>
#include <iomanip>
#include <iostream>
#include <span>
#include <stop_token>
#include <string>
#include <thread>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/heartbeat/HeartbeatRecv.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/message/CHP1Message.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation;
using namespace constellation::heartbeat;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::utils;
using namespace std::chrono_literals;
using namespace std::string_literals;

namespace {
    // Use global std::function to work around C linkage
    std::function<void(int)> signal_handler_f {}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
} // namespace

extern "C" void signal_hander(int signal) {
    signal_handler_f(signal);
}

namespace {
    void cli_loop(std::span<char*> args) {
        // Get group via cmdline
        std::cout << "Usage: chp_receiver CONSTELLATION_GROUP\n" << std::flush;

        auto group = "constellation"s;
        if(args.size() >= 2) {
            group = args[1];
        }
        std::cout << "Using constellation group " << std::quoted(group) << "\n" << std::flush;

        auto chirp_manager = chirp::Manager("255.255.255.255", "0.0.0.0", group, "chp_receiver");
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
    }
} // namespace

int main(int argc, char* argv[]) {
    try {
        cli_loop(std::span(argv, argc));
    } catch(...) {
        return 1;
    }
    return 0;
}
