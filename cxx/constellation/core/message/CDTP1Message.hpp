/**
 * @file
 * @brief Message class for CDTP1
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
#include <utility>
#include <vector>

#include <zmq_addon.hpp>

#include "constellation/build.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/BaseHeader.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/protocol/Protocol.hpp"

namespace constellation::message {

    /** Class representing a CDTP1 message */
    class CDTP1Message {
    public:
        enum class Type : std::uint8_t {
            DATA = '\x00',
            BOR = '\x01',
            EOR = '\x02',
        };

        class CNSTLN_API Header final : public BaseHeader {
        public:
            Header(std::string sender,
                   std::uint64_t seq,
                   Type type,
                   std::chrono::system_clock::time_point time = std::chrono::system_clock::now(),
                   config::Dictionary tags = {})
                : BaseHeader(protocol::CDTP1, std::move(sender), time, std::move(tags)), seq_(seq), type_(type) {}

            constexpr std::uint64_t getSequenceNumber() const { return seq_; }

            constexpr Type getType() const { return type_; }

            CNSTLN_API std::string to_string() const final;

            CNSTLN_API static Header disassemble(std::span<const std::byte> data);

            CNSTLN_API void msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const final;

        private:
            Header(std::string sender,
                   std::chrono::system_clock::time_point time,
                   config::Dictionary tags,
                   std::uint64_t seq,
                   Type type)
                : BaseHeader(protocol::CDTP1, std::move(sender), time, std::move(tags)), seq_(seq), type_(type) {}

        private:
            std::uint64_t seq_;
            Type type_;
        };

    public:
        /**
         * @param header CDTP1 header of the message
         * @param frames Number of payload frames to reserve
         */
        CNSTLN_API CDTP1Message(Header header, std::size_t frames = 1);

        /**
         * @return Read-only reference to the CSCP1 header of the message
         */
        constexpr const Header& getHeader() const { return header_; }

        /**
         * @return Reference to the CSCP1 header of the message
         */
        constexpr Header& getHeader() { return header_; }

        /**
         * @return Read-only reference to the payload of the message
         */
        const std::vector<message::PayloadBuffer>& getPayload() const { return payload_buffers_; }

        /**
         * @param payload Payload buffer containing a payload to be added as ZeroMQ message
         */
        void addPayload(message::PayloadBuffer&& payload) { payload_buffers_.emplace_back(std::move(payload)); }

        /**
         * @return Current number of payload frames in this message
         */
        std::size_t countPayloadFrames() const { return payload_buffers_.size(); }

        /**
         * Assemble full message to frames for ZeroMQ
         *
         * This function always moves the payload
         */
        CNSTLN_API zmq::multipart_t assemble();

        /**
         * Disassemble message from ZeroMQ frames
         *
         * This function moves the payload frames
         */
        CNSTLN_API static CDTP1Message disassemble(zmq::multipart_t& frames);

    private:
        Header header_;
        std::vector<message::PayloadBuffer> payload_buffers_;
    };

} // namespace constellation::message
