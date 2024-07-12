/**
 * @file
 * @brief CHIRP service
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <asio/ip/address_v4.hpp>

#include "constellation/build.hpp"
#include "constellation/core/utils/networking.hpp"

namespace constellation::chirp {

    class CHIRPService {
    public:
        /** CHIRP service identifier */
        enum class Identifier : std::uint8_t {
            /** The ANY service identifier is used in a request to get replies from any service */
            ANY = '\x00',

            /** The CONTROL service identifier indicates a CSCP (Constellation Satellite Control Protocol) service */
            CONTROL = '\x01',

            /** The HEARTBEAT service identifier indicates a CHP (Constellation Heartbeat Protocol) service */
            HEARTBEAT = '\x02',

            /** The MONITORING service identifier indicates a CMDP (Constellation Monitoring Distribution Protocol) service
             */
            MONITORING = '\x03',

            /** The DATA service identifier indicates a CDTP (Constellation Data Transmission Protocol) service */
            DATA = '\x04',
        };
        using enum Identifier;

    public:
        /**
         * @brief Construct new CHIRP service
         *
         * @param group_name Name of the CHIRP group
         * @param host_name Name of the CHIRP host
         * @param service_identifier Identifier of the service
         * @param port Port of the service
         * @param address Address of the service (defaults to loopback address)
         */
        CHIRPService(std::string group_name,
                     std::string host_name,
                     Identifier service_identifier,
                     utils::Port port,
                     asio::ip::address_v4 address = asio::ip::address_v4::loopback())
            : group_name_(std::move(group_name)), host_name_(std::move(host_name)), service_identifier_(service_identifier),
              port_(port), address_(std::move(address)) {}

        bool operator<=>(const CHIRPService& other) const;

        /**
         * @return Name of the CHIRP group
         */
        std::string_view getGroupName() const { return group_name_; }

        /**
         * @return Name of the host
         */
        std::string_view getHostName() const { return host_name_; }

        /**
         * @return Identifier of the service
         */
        constexpr Identifier getServiceIdentifier() const { return service_identifier_; }

        /**
         * @return Port of the service
         */
        constexpr utils::Port getPort() const { return port_; }

        /**
         * @return Address of the service host
         */
        asio::ip::address_v4 getAddress() const { return address_; }

        /**
         * @return URI of the service in the form "tcp://<ip>:<port>"
         */
        std::string getURI() const { return utils::endpoint_to_uri("tcp", address_, port_); }

    private:
        std::string group_name_;
        std::string host_name_;
        Identifier service_identifier_;
        utils::Port port_;
        asio::ip::address_v4 address_;
    };

} // namespace constellation::chirp
