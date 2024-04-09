/**
 * @file
 * @brief Message class for CDTP1
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/config.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/BaseHeader.hpp"
#include "constellation/core/message/Protocol.hpp"

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
                   std::chrono::system_clock::time_point time = std::chrono::system_clock::now())
                : BaseHeader(CDTP1, std::move(sender), time), seq_(seq), type_(type) {}

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
                : BaseHeader(CDTP1, std::move(sender), time, std::move(tags)), seq_(seq), type_(type) {}

        private:
            std::uint64_t seq_;
            Type type_;
        };

    public:
        /**
         * @param header CDTP1 header of the message
         * @param frames Number of payload frames to reserve
         */
        CNSTLN_API CDTP1Message(Header header, size_t frames = 1);

        constexpr const Header& getHeader() const { return header_; }

        std::vector<std::shared_ptr<zmq::message_t>> getPayload() const { return payload_frames_; }

        void addPayload(std::shared_ptr<zmq::message_t> payload) { payload_frames_.emplace_back(std::move(payload)); }

        /**
         * Assemble full message to frames for ZeroMQ
         *
         * This function moves the payload.
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
        std::vector<std::shared_ptr<zmq::message_t>> payload_frames_;
    };

} // namespace constellation::message
