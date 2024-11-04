/**
 * @file
 * @brief CHIRP Message
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "constellation/build.hpp"
#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/utils/networking.hpp"

namespace constellation::message {

    /** MD5 hash stored as array with 16 bytes */
    class MD5Hash : public std::array<std::uint8_t, 16> {
    public:
        constexpr MD5Hash() = default;

        /**
         * Construct MD5 hash from a string
         *
         * @param string String from which to create the MD5 hash
         */
        CNSTLN_API MD5Hash(std::string_view string);

        /**
         * Convert MD5 hash to an human readable string
         *
         * @return String containing a lowercase hex representation of the MD5 hash
         */
        CNSTLN_API std::string to_string() const;

        CNSTLN_API bool operator<(const MD5Hash& other) const;
    };

    /** CHIRP message assembled to array of bytes */
    using AssembledMessage = std::array<std::byte, chirp::CHIRP_MESSAGE_LENGTH>;

    /** CHIRP message */
    class CHIRPMessage {
    public:
        /**
         * @param type CHIRP message type
         * @param group_id Constellation group ID (MD5 Hash of group name)
         * @param host_id Constellation host ID (MD5 Hash of host name)
         * @param service_id CHIRP service identifier
         * @param port Service port
         */
        CNSTLN_API
        CHIRPMessage(chirp::MessageType type,
                     MD5Hash group_id,
                     MD5Hash host_id,
                     chirp::ServiceIdentifier service_id,
                     utils::Port port);

        /**
         * @param type CHIRP message type
         * @param group Constellation group name (converted to group ID using `MD5Hash`)
         * @param host Host name (converted to host ID using `MD5Hash`)
         * @param service_id CHIRP service identifier
         * @param port Service port
         */
        CNSTLN_API
        CHIRPMessage(chirp::MessageType type,
                     std::string_view group,
                     std::string_view host,
                     chirp::ServiceIdentifier service_id,
                     utils::Port port);

        /** Return the message type */
        constexpr chirp::MessageType getType() const { return type_; }

        /** Return the group ID of the message */
        constexpr MD5Hash getGroupID() const { return group_id_; }

        /** Return the host ID of the message */
        constexpr MD5Hash getHostID() const { return host_id_; }

        /** Return the service identifier of the message */
        constexpr chirp::ServiceIdentifier getServiceIdentifier() const { return service_id_; }

        /** Return the service port of the message */
        constexpr utils::Port getPort() const { return port_; }

        /** Assemble message to byte array */
        CNSTLN_API AssembledMessage assemble() const;

        /**
         * Constructor for a CHIRP message from an assembled message
         *
         * @param assembled_message View of assembled message
         * @throw DecodeError If the message header does not match the CHIRP specification, or if the message
         * has an unknown `ServiceIdentifier`
         */
        CNSTLN_API static CHIRPMessage disassemble(std::span<const std::byte> assembled_message);

    private:
        CHIRPMessage() = default;

    private:
        chirp::MessageType type_;
        MD5Hash group_id_;
        MD5Hash host_id_;
        chirp::ServiceIdentifier service_id_;
        utils::Port port_;
    };

} // namespace constellation::message
