/**
 * @file
 * @brief Implementation of CDTP1 message type
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "CDTP1Message.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include <msgpack.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/BaseHeader.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/protocol/Protocol.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/msgpack.hpp"
#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::protocol;
using namespace constellation::utils;
using namespace std::string_view_literals;

// Similar to BaseHeader::disassemble in BaseHeader.cpp, check when modifying
CDTP1Message::Header CDTP1Message::Header::disassemble(std::span<const std::byte> data) {
    try {
        // Offset since we decode four separate msgpack objects
        std::size_t offset = 0;

        // Unpack protocol
        const auto protocol_identifier = msgpack_unpack_to<std::string>(to_char_ptr(data.data()), data.size_bytes(), offset);

        // Try to decode protocol identifier into protocol
        Protocol protocol_recv {};
        try {
            protocol_recv = get_protocol(protocol_identifier);
        } catch(std::invalid_argument& e) {
            throw InvalidProtocolError(e.what());
        }

        if(protocol_recv != CDTP1) {
            throw UnexpectedProtocolError(protocol_recv, CDTP1);
        }

        // Unpack sender
        const auto sender = msgpack_unpack_to<std::string>(to_char_ptr(data.data()), data.size_bytes(), offset);

        // Unpack time
        const auto time =
            msgpack_unpack_to<std::chrono::system_clock::time_point>(to_char_ptr(data.data()), data.size_bytes(), offset);

        // Unpack message type
        const auto type =
            static_cast<Type>(msgpack_unpack_to<std::uint8_t>(to_char_ptr(data.data()), data.size_bytes(), offset));
        // TODO(stephan.lachnit): check range and throw if outside

        // Unpack sequence number
        const auto seq = msgpack_unpack_to<std::uint64_t>(to_char_ptr(data.data()), data.size_bytes(), offset);

        // Unpack tags
        const auto tags = msgpack_unpack_to<Dictionary>(to_char_ptr(data.data()), data.size_bytes(), offset);

        // Construct header
        return {sender, time, tags, seq, type};
    } catch(const MsgPackError& e) {
        throw MessageDecodingError(e.what());
    }
}

void CDTP1Message::Header::msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const {
    // first pack version
    msgpack_packer.pack(get_protocol_identifier(CDTP1));
    // then sender
    msgpack_packer.pack(getSender());
    // then time
    msgpack_packer.pack(getTime());
    // then type
    msgpack_packer.pack(std::to_underlying(type_));
    // then seq
    msgpack_packer.pack(seq_);
    // then tags
    msgpack_packer.pack(getTags());
}

std::string CDTP1Message::Header::to_string() const {
    // Insert type and sequence number into string from base class function
    std::ostringstream insert {};
    insert << "\nType:   " << type_ //
           << "\nSeq No: " << seq_; //

    // Insert before tags (at least 59 chars after string begin)
    auto out = BaseHeader::to_string();
    auto pos = out.find("\nTags:", 59);
    out.insert(pos, insert.str());

    return out;
}

CDTP1Message::CDTP1Message(Header header, std::size_t frames) : header_(std::move(header)) {
    payload_buffers_.reserve(frames);
}

zmq::multipart_t CDTP1Message::assemble() {
    zmq::multipart_t frames {};

    // First frame: header
    msgpack::sbuffer sbuf_header {};
    msgpack_pack(sbuf_header, header_);
    frames.add(PayloadBuffer(std::move(sbuf_header)).to_zmq_msg_release());

    // Second frame until Nth frame: always move payload (no reuse)
    for(auto& PayloadBuffer : payload_buffers_) {
        frames.add(PayloadBuffer.to_zmq_msg_release());
    }
    // clear payload_frames_ member as payload buffers has been released
    payload_buffers_.clear();
    return frames;
}

CDTP1Message CDTP1Message::disassemble(zmq::multipart_t& frames) {
    // Decode header
    const auto header_frame = frames.pop();
    const auto header = Header::disassemble({to_byte_ptr(header_frame.data()), header_frame.size()});

    // Create message, reversing space for frames
    auto cdtp_message = CDTP1Message(header, frames.size());

    // Move payload frames into buffers
    while(!frames.empty()) {
        cdtp_message.addPayload(frames.pop());
    }

    // If BOR / EOR, exactly one frame needed
    if((header.getType() == Type::BOR || header.getType() == Type::EOR) && cdtp_message.getPayload().size() != 1) {
        throw MessageDecodingError("Wrong number of frames for " + to_string(header.getType()) +
                                   ", exactly one payload frame expected");
    }

    return cdtp_message;
}
