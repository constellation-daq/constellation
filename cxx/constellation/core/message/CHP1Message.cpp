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
        const auto msgpack_protocol_identifier = msgpack::unpack(to_char_ptr(frame.data()), frame.size(), offset);
        const auto protocol_identifier = msgpack_protocol_identifier->as<std::string>();

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
        const auto msgpack_sender = msgpack::unpack(to_char_ptr(frame.data()), frame.size(), offset);
        const auto sender = msgpack_sender->as<std::string>();

        // Unpack time
        const auto msgpack_time = msgpack::unpack(to_char_ptr(frame.data()), frame.size(), offset);
        const auto time = msgpack_time->as<std::chrono::system_clock::time_point>();

        // Unpack remote state
        const auto msgpack_state = msgpack::unpack(to_char_ptr(frame.data()), frame.size(), offset);
        const auto state = static_cast<CSCP::State>(msgpack_state->as<std::uint8_t>());

        // Unpack time interval
        const auto msgpack_interval = msgpack::unpack(to_char_ptr(frame.data()), frame.size(), offset);
        const auto interval = static_cast<std::chrono::milliseconds>(msgpack_interval->as<std::uint16_t>());

        // Construct message
        return {sender, state, interval, time};
    } catch(const msgpack::type_error&) {
        throw MessageDecodingError("malformed data");
    } catch(const msgpack::unpack_error&) {
        throw MessageDecodingError("could not unpack data");
    }
}

zmq::multipart_t CHP1Message::assemble() {
    zmq::multipart_t frames {};

    msgpack::sbuffer sbuf {};

    // first pack protocol
    msgpack::pack(sbuf, get_protocol_identifier(CHP1));
    // then sender
    msgpack::pack(sbuf, getSender());
    // then time
    msgpack::pack(sbuf, getTime());
    // then state
    msgpack::pack(sbuf, std::to_underlying(state_));
    // then interval
    msgpack::pack(sbuf, static_cast<uint16_t>(interval_.count()));

    frames.add(PayloadBuffer(std::move(sbuf)).to_zmq_msg_release());

    return frames;
}
