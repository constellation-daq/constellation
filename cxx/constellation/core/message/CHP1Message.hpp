/**
 * @file
 * @brief Message class for CHP1
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <utility>

#include <zmq_addon.hpp>

#include "constellation/build.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/protocol/Protocol.hpp"

namespace constellation::message {

    /** Class representing a CHP1 message */
    class CHP1Message {
    public:
        /**
         * @param sender Sender name
         * @param time Message time
         * @param state State of the sender
         * @param interval Time interval until next message is expected
         */
        CHP1Message(std::string sender,
                    protocol::CSCP::State state,
                    std::chrono::milliseconds interval,
                    std::chrono::system_clock::time_point time = std::chrono::system_clock::now())
            : sender_(std::move(sender)), time_(time), state_(state), interval_(interval) {}

        /** Return message protocol */
        constexpr protocol::Protocol getProtocol() const { return protocol_; }

        /** Return message sender */
        std::string_view getSender() const { return sender_; }

        /** Return message time */
        constexpr std::chrono::system_clock::time_point getTime() const { return time_; }

        /** Return state of the message */
        constexpr protocol::CSCP::State getState() const { return state_; }

        /** Return maxima time interval until next message is expected */
        constexpr std::chrono::milliseconds getInterval() const { return interval_; }

        /**
         * Assemble full message to frames for ZeroMQ
         */
        CNSTLN_API zmq::multipart_t assemble();

        /**
         * Disassemble message from ZeroMQ frames
         *
         * This function moves the frames
         */
        CNSTLN_API static CHP1Message disassemble(zmq::multipart_t& frames);

    private:
        protocol::Protocol protocol_ {protocol::CHP1};
        std::string sender_;
        std::chrono::system_clock::time_point time_;
        protocol::CSCP::State state_;
        std::chrono::milliseconds interval_;
    };

} // namespace constellation::message
