/**
 * @file
 * @brief Implementation of CHP1 message type
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "CHP1Message.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

#include <msgpack.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/protocol/Protocol.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/msgpack.hpp"
#include "constellation/core/utils/std_future.hpp"

using namespace constellation::message;
using namespace constellation::utils;
using namespace constellation::protocol;
using namespace std::string_view_literals;

CHP1Message CHP1Message::disassemble(zmq::multipart_t& frames) {
    if(frames.size() != 1) {
        throw MessageDecodingError("CHP1 messages can only have one frame");
    }

    const auto frame = frames.pop();

    try {
        // Offset since we decode five separate msgpack objects
        std::size_t offset = 0;

        // Unpack protocol
        const auto protocol_identifier = msgpack_unpack_to<std::string>(to_char_ptr(frame.data()), frame.size(), offset);

        // Try to decode protocol identifier into protocol
        Protocol protocol_recv {};
        try {
            protocol_recv = get_protocol(protocol_identifier);
        } catch(const std::invalid_argument& error) {
            throw InvalidProtocolError(error.what());
        }

        if(protocol_recv != CHP1) {
            throw UnexpectedProtocolError(protocol_recv, CHP1);
        }

        // Unpack sender
        const auto sender = msgpack_unpack_to<std::string>(to_char_ptr(frame.data()), frame.size(), offset);

        // Unpack time
        const auto time =
            msgpack_unpack_to<std::chrono::system_clock::time_point>(to_char_ptr(frame.data()), frame.size(), offset);

        // Unpack remote state
        const auto state =
            static_cast<CSCP::State>(msgpack_unpack_to<std::uint8_t>(to_char_ptr(frame.data()), frame.size(), offset));

        // Unpack time interval
        const auto interval = static_cast<std::chrono::milliseconds>(
            msgpack_unpack_to<std::uint16_t>(to_char_ptr(frame.data()), frame.size(), offset));

        // Construct message
        return {sender, state, interval, time};
    } catch(const MsgpackUnpackError& e) {
        throw MessageDecodingError(e.what());
    }
}

zmq::multipart_t CHP1Message::assemble() {
    zmq::multipart_t frames {};

    msgpack::sbuffer sbuf {};

    // first pack protocol
    msgpack_pack(sbuf, get_protocol_identifier(CHP1));
    // then sender
    msgpack_pack(sbuf, getSender());
    // then time
    msgpack_pack(sbuf, getTime());
    // then state
    msgpack_pack(sbuf, std::to_underlying(state_));
    // then interval
    msgpack_pack(sbuf, static_cast<uint16_t>(interval_.count()));

    frames.add(PayloadBuffer(std::move(sbuf)).to_zmq_msg_release());

    return frames;
}
