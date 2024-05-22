/**
 * @file
 * @brief CMDP log sender
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <iostream>
#include <string>

#include "constellation/core/logging/log.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/core/logging/SinkManager.hpp"

using namespace constellation::log;

int main(int argc, char* argv[]) {
    // Get topic via cmdline
    std::string topic = "test";
    if(argc >= 2) {
        topic = argv[1];
    }

    // Only log to CMDP
    SinkManager::getInstance().setGlobalConsoleLevel(OFF);
    SinkManager::getInstance().setCMDPLevelsCustom(TRACE);

    const Logger logger {std::move(topic)};
    std::cout << "Starting logging on port " << SinkManager::getInstance().getCMDPPort() << std::endl;

    while(true) {
        std::string message;
        std::cout << "send message: ";
        std::getline(std::cin, message);

        LOG(logger, TRACE) << message;
    }

    return 0;
}
