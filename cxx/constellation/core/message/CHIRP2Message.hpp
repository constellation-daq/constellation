/**
 * @file
 * @brief CHIRP Message
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "constellation/build.hpp"
#include "constellation/core/chirp/CHIRPService.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/utils/networking.hpp"

namespace constellation::message {

    /** CHIRP message */
    class CHIRP2Message {
    public:
        enum class Type : std::uint8_t {
            REQUEST = '\x00',
            OFFER = '\x01',
            DEPART = '\x02',
        };

    public:
        /**
         * @param group_name Name of the CHIRP group
         * @param host_name Name of the host
         * @param type CHIRP message type
         * @param service_identifier CHIRP service identifier
         * @param port Service port
         */
        CNSTLN_API CHIRP2Message(std::string group_name,
                                 std::string host_name,
                                 Type type,
                                 chirp::CHIRPService::Identifier service_identifier,
                                 utils::Port port);

        /** Return the CHIRP group of the message */
        constexpr std::string_view getGroupName() const { return group_name_; }

        /** Return the host name of the message */
        constexpr std::string_view getHostName() const { return host_name_; }

        /** Return the message type */
        constexpr Type getType() const { return type_; }

        /** Return the service identifier of the message */
        constexpr chirp::CHIRPService::Identifier getServiceIdentifier() const { return service_identifier_; }

        /** Return the service port of the message */
        constexpr utils::Port getPort() const { return port_; }

        /** Assemble message to bytes */
        CNSTLN_API PayloadBuffer assemble() const;

        /** Constructor for a CHIRP message from bytes */
        CNSTLN_API static CHIRP2Message disassemble(std::span<const std::byte> bytes);

    private:
        std::string group_name_;
        std::string host_name_;
        Type type_;
        chirp::CHIRPService::Identifier service_identifier_;
        utils::Port port_;
    };

} // namespace constellation::message
