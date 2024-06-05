/**
 * @file
 * @brief Example implementation of CHIRP broadcast receiver
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <charconv>
#include <cstring>
#include <iomanip>
#include <iostream>

#include <asio.hpp>

#include "constellation/core/chirp/BroadcastRecv.hpp"
#include "constellation/core/chirp/CHIRP_definitions.hpp"

using namespace constellation::chirp;

int main(int argc, char* argv[]) {
    // Specify any address and port via cmdline
    asio::ip::address any_address = asio::ip::address_v4::any();
    asio::ip::port_type port = CHIRP_PORT;
    if(argc >= 2) {
        try {
            any_address = asio::ip::make_address(argv[1]);
        } catch(const asio::system_error& error) {
            std::cerr << "Unable to use specified any address " << std::quoted(argv[1]) << ", using default instead"
                      << std::endl;
        }
    }
    if(argc >= 3) {
        std::from_chars(argv[2], argv[2] + std::strlen(argv[2]), port);
    }

    BroadcastRecv receiver {any_address, port};

    while(true) {
        auto message = receiver.recvBroadcast();
        std::cout << "Received message from " << message.address.to_string() << ": " << message.to_string() << std::endl;
    }

    return 0;
}
