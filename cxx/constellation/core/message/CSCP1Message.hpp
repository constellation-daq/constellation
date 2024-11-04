/**
 * @file
 * @brief Message class for CSCP1
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <zmq_addon.hpp>

#include "constellation/build.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/BaseHeader.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/protocol/Protocol.hpp"

namespace constellation::message {

    /** Class representing a CSCP1 message */
    class CSCP1Message {
    public:
        /** Enum describing the type of CSCP1 message */
        enum class Type : std::uint8_t {
            /** Request with a command */
            REQUEST = '\x00',

            /** Command is being executed */
            SUCCESS = '\x01',

            /** Command is valid but not implemented */
            NOTIMPLEMENTED = '\x02',

            /** Command is valid but mandatory payload information is missing or incorrectly formatted */
            INCOMPLETE = '\x03',

            /** Command is invalid for the current state */
            INVALID = '\x04',

            /** Command is entirely unknown */
            UNKNOWN = '\x05',

            /** Previously received message is invalid */
            ERROR = '\x06',
        };

        /** CSCP1 Header */
        class CNSTLN_API Header final : public BaseHeader {
        public:
            Header(std::string sender,
                   std::chrono::system_clock::time_point time = std::chrono::system_clock::now(),
                   config::Dictionary tags = {})
                : BaseHeader(protocol::CSCP1, std::move(sender), time, std::move(tags)) {}

            static Header disassemble(std::span<const std::byte> data) {
                return {BaseHeader::disassemble(protocol::CSCP1, data)};
            }

        private:
            Header(BaseHeader&& base_header) : BaseHeader(std::move(base_header)) {}
        };

    public:
        /**
         * @param header CSCP1 header of the message
         * @param verb Message verb containing the type and the command/reply string
         */
        CNSTLN_API CSCP1Message(Header header, std::pair<Type, std::string> verb);

        /**
         * @return Read-only reference to the CSCP1 header of the message
         */
        constexpr const Header& getHeader() const { return header_; }

        /**
         * @return Reference to the CSCP1 header of the message
         */
        constexpr Header& getHeader() { return header_; }

        /**
         * @return Message verb containing the type and the command/reply string
         */
        std::pair<Type, std::string_view> getVerb() const { return verb_; }

        /**
         * @return Message payload
         */
        const message::PayloadBuffer& getPayload() const { return payload_; }

        /**
         * @return True if message has payload
         */
        bool hasPayload() const { return !payload_.empty(); }

        /**
         * @param payload Payload buffer containing the payload to be added as ZeroMQ message
         */
        void addPayload(message::PayloadBuffer&& payload) { payload_ = std::move(payload); }

        /**
         * Assemble full message to frames for ZeroMQ
         *
         * @param keep_payload If true, the payload is kept such that the message can be send again
         *
         * @return Message assembled to ZeroMQ frames
         */
        CNSTLN_API zmq::multipart_t assemble(bool keep_payload = false);

        /**
         * Disassemble message from ZeroMQ frames
         *
         * This function moves the payload frame if there is one.
         *
         * @return New CSCP1Message assembled from ZeroMQ frames
         * @throw TODO
         */
        CNSTLN_API static CSCP1Message disassemble(zmq::multipart_t& frames);

    private:
        Header header_;
        std::pair<Type, std::string> verb_;
        message::PayloadBuffer payload_;
    };

} // namespace constellation::message
