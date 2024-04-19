/**
 * @file
 * @brief Base class for message headers in CMDP, CDTP and CSCP
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include <msgpack/pack_decl.hpp>
#include <msgpack/sbuffer_decl.hpp>

#include "constellation/core/config.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/Protocol.hpp"

namespace constellation::message {

    /** Message Header Base Class */
    class CNSTLN_API BaseHeader {
    public:
        virtual ~BaseHeader() = default;

        // Default copy/move constructor/assignment
        BaseHeader(const BaseHeader& other) = default;
        BaseHeader& operator=(const BaseHeader& other) = default;
        BaseHeader(BaseHeader&& other) noexcept = default;
        BaseHeader& operator=(BaseHeader&& other) = default;

        /** Return message protocol */
        constexpr Protocol getProtocol() const { return protocol_; }

        /** Return message sender */
        std::string_view getSender() const { return sender_; }

        /** Return message time */
        constexpr std::chrono::system_clock::time_point getTime() const { return time_; }

        /** Return message tags */
        const config::Dictionary& getTags() const { return tags_; }

        /** Return message tag */
        config::Value getTag(const std::string& key) const { return tags_.at(key); }

        /** Set message tag */
        void setTag(const std::string& key, config::Value value) { tags_[key] = std::move(value); }

        /** Convert message header to human readable string */
        CNSTLN_API virtual std::string to_string() const;

        /** Pack message header with msgpack */
        CNSTLN_API virtual void msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const;

    protected:
        /**
         * Construct new message header
         *
         * @param protocol Message protocol
         * @param sender Sender name
         * @param time Message time
         * @param tags Message tags (defaults to empty dictionary)
         */
        BaseHeader(Protocol protocol,
                   std::string sender,
                   std::chrono::system_clock::time_point time,
                   config::Dictionary tags = {})
            : protocol_(protocol), sender_(std::move(sender)), time_(time), tags_(std::move(tags)) {}

        /**
         * Disassemble message from from bytes
         *
         * @param protocol Protocol
         * @param data View to byte data
         */
        CNSTLN_API static BaseHeader disassemble(Protocol protocol, std::span<const std::byte> data);

    private:
        Protocol protocol_;
        std::string sender_;
        std::chrono::system_clock::time_point time_;
        config::Dictionary tags_;
    };

} // namespace constellation::message
