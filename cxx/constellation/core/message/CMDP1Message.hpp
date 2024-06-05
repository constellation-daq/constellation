/**
 * @file
 * @brief Message class for CMDP1
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

#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/build.hpp"
#include "constellation/core/logging/Level.hpp"
#include "constellation/core/message/BaseHeader.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/message/Protocol.hpp"

namespace constellation::message {

    /** Class representing a CMDP1 message */
    class CMDP1Message {
    public:
        /** CMDP1 Header */
        class CNSTLN_API Header final : public BaseHeader {
        public:
            Header(std::string sender, std::chrono::system_clock::time_point time = std::chrono::system_clock::now())
                : BaseHeader(CMDP1, std::move(sender), time) {}

            static Header disassemble(std::span<const std::byte> data) { return {BaseHeader::disassemble(CMDP1, data)}; }

        private:
            Header(BaseHeader&& base_header) : BaseHeader(std::move(base_header)) {}
        };

    public:
        /**
         * @return CMDP1 header of the message
         */
        constexpr const Header& getHeader() const { return header_; }

        /**
         * @return CMDP message topic
         */
        std::string_view getTopic() const { return topic_; };

        /**
         * @return If the message is a log message
         */
        CNSTLN_API bool isLogMessage() const;

        /**
         * Assemble full message to frames for ZeroMQ
         *
         * This function moves the payload.
         *
         * @return Message assembled to ZeroMQ frames
         */
        CNSTLN_API zmq::multipart_t assemble();

        /**
         * Disassemble message from ZeroMQ frames
         *
         * This function moves the payload.
         *
         * @return New CMDP1Message assembled from ZeroMQ frames
         * @throw MessageDecodingError If the message is not a valid CMDP1 message
         */
        CNSTLN_API static CMDP1Message disassemble(zmq::multipart_t& frames);

    protected:
        /**
         * Construct a new CMDP1 message
         *
         * @param topic Topic of the message
         * @param header Header of the message
         * @param payload Payload of the message
         */
        CMDP1Message(std::string topic, Header header, message::PayloadBuffer&& payload);

        /**
         * @return Message payload
         */
        const message::PayloadBuffer& get_payload() const { return payload_; }

        /**
         * Extract the log level from CMDP1 message topic
         *
         * @param topic Topic of the message
         * @throw MessageDecodingError If not a valid log level
         */
        static log::Level get_log_level_from_topic(std::string_view topic);

    private:
        std::string topic_;
        Header header_;
        message::PayloadBuffer payload_;
    };

    class CMDP1LogMessage : public CMDP1Message {
    public:
        /**
         * Construct a new CMDP1 message for logging
         *
         * @param level Log level of the message
         * @param log_topic Log topic of the message (can be empty)
         * @param header CMDP1 header of the message
         * @param message Log message
         */
        CNSTLN_API CMDP1LogMessage(log::Level level, std::string log_topic, Header header, std::string message);

        /**
         * Construct a CMDP1LogMessage from a decoded CMDP1Message
         *
         * @throw IncorrectMessageType If the message is not a (valid) log message
         */
        CNSTLN_API CMDP1LogMessage(CMDP1Message&& message);

        /**
         * @return Log level of the message
         */
        constexpr log::Level getLogLevel() const { return level_; }

        /**
         * @return Log topic of the message (might be empty)
         */
        std::string_view getLogTopic() const { return log_topic_; }

        /**
         * @return Log message
         */
        CNSTLN_API std::string_view getLogMessage() const;

        /**
         * Disassemble log message from ZeroMQ frames
         *
         * This function moves the payload.
         *
         * @return New CMDP1LogMessage assembled from ZeroMQ frames
         * @throw MessageDecodingError If the message is not a valid CMDP1 message
         * @throw IncorrectMessageType If the message is a valid CMDP1 message but not a (valid) log message
         */
        CNSTLN_API static CMDP1LogMessage disassemble(zmq::multipart_t& frames);

    private:
        log::Level level_;
        std::string log_topic_;
    };

} // namespace constellation::message
