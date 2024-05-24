/**
 * @file
 * @brief Config parser tool
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

#include "constellation/controller/ConfigParser.hpp"

using namespace constellation;
using namespace constellation::controller;
using namespace std::literals::chrono_literals;

int main(int argc, char* argv[]) {
    // Get address via cmdline
    if(argc != 2) {
        std::cout << "Invalid usage: config_parser FILE" << std::endl;
        return 1;
    }

    try {
        ConfigParser cfg(argv[1]);

    } catch(const utils::RuntimeError& err) {
        std::cout << "Error:\n" << err.what();
        return 1;
    }

    return 0;
}
