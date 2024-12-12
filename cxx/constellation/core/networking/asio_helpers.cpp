/**
 * @file
 * @brief Implementation of Asio helper functions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "asio_helpers.hpp"

#include <set>
#include <string>
#include <string_view>

#include <asio.hpp>
#ifndef _WIN32
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "constellation/core/networking/Port.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::networking;
using namespace constellation::utils;

std::set<asio::ip::address_v4> constellation::networking::get_broadcast_addresses() {
    std::set<asio::ip::address_v4> addresses {};

#if defined(_WIN32)

    // TODO(stephan.lachnit): implement this on Windows, right now take default brd address
    addresses.emplace(asio::ip::address_v4::broadcast());

#else

    // Obtain linked list of all local network interfaces
    struct ifaddrs* addrs = nullptr;
    struct ifaddrs* ifa = nullptr;
    if(getifaddrs(&addrs) != 0) {
        return {};
    }

    // Iterate through list of interfaces
    for(ifa = addrs; ifa != nullptr; ifa = ifa->ifa_next) {

        // Select only running interfaces and those providing IPV4:
        if(ifa->ifa_addr == nullptr || ((ifa->ifa_flags & IFF_RUNNING) == 0U) || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        // Ensure that the interface holds a broadcast address
        if((ifa->ifa_flags & IFF_BROADCAST) == 0U) {
            continue;
        }

        char buffer[NI_MAXHOST];           // NOLINT(modernize-avoid-c-arrays)
        if(getnameinfo(ifa->ifa_broadaddr, // NOLINT(cppcoreguidelines-pro-type-union-access)
                       sizeof(struct sockaddr_in),
                       buffer, // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
                       sizeof(buffer),
                       nullptr,
                       0,
                       NI_NUMERICHOST) == 0) {

            try {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
                addresses.emplace(asio::ip::make_address_v4(buffer));
            } catch(const asio::system_error& error) {
                continue;
            }
        }
    }

    freeifaddrs(addrs);

#endif

    return addresses;
}

std::string constellation::networking::to_uri(const asio::ip::address_v4& address, Port port, std::string_view protocol) {
    std::string uri {};
    if(!protocol.empty()) {
        uri += protocol;
        uri += "://";
    }
    uri += range_to_string(address.to_bytes(), ".");
    uri += ":";
    uri += to_string(port);
    return uri;
}
