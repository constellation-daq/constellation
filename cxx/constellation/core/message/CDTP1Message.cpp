/**
 * @file
 * @brief Implementation of CDTP1 message type
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "CDTP1Message.hpp"

#include <magic_enum.hpp>
#include <msgpack.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/message/Protocol.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/std23.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::utils;
using namespace std::literals::string_view_literals;

// Similar to Header::disassemble in Header.tpp, check when modifying
CDTP1Message::Header CDTP1Message::Header::disassemble(std::span<const std::byte> data) {
    // Offset since we decode four separate msgpack objects
    std::size_t offset = 0;

    // Unpack protocol
    const auto msgpack_protocol_identifier = msgpack::unpack(to_char_ptr(data.data()), data.size_bytes(), offset);
    const auto protocol_identifier = msgpack_protocol_identifier->as<std::string>();

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
    const auto msgpack_sender = msgpack::unpack(to_char_ptr(data.data()), data.size_bytes(), offset);
    const auto sender = msgpack_sender->as<std::string>();

    // Unpack time
    const auto msgpack_time = msgpack::unpack(to_char_ptr(data.data()), data.size_bytes(), offset);
    const auto time = msgpack_time->as<std::chrono::system_clock::time_point>();

    // Unpack message type
    const auto msgpack_type = msgpack::unpack(to_char_ptr(data.data()), data.size_bytes(), offset);
    const auto type = static_cast<Type>(msgpack_type->as<std::uint8_t>());
    // TODO(stephan.lachnit): check range and throw if outside

    // Unpack sequence number
    const auto msgpack_seq = msgpack::unpack(to_char_ptr(data.data()), data.size_bytes(), offset);
    const auto seq = msgpack_seq->as<std::uint64_t>();

    // Unpack tags
    const auto msgpack_tags = msgpack::unpack(to_char_ptr(data.data()), data.size_bytes(), offset);
    const auto tags = msgpack_tags->as<Dictionary>();

    // Construct header
    return {sender, time, tags, seq, type};
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
    insert << "\nType:   "sv << magic_enum::enum_name(type_) //
           << "\nSeq No: "sv << seq_;                        //

    // Insert before tags (at least 59 chars after string begin)
    auto out = BaseHeader::to_string();
    auto pos = out.find("\nTags:", 59);
    out.insert(pos, insert.str());

    return out;
}

CDTP1Message::CDTP1Message(Header header, size_t frames) : header_(std::move(header)) {
    payload_frames_.reserve(frames);
}

zmq::multipart_t CDTP1Message::assemble() {
    zmq::multipart_t frames {};

    // First frame: header
    msgpack::sbuffer sbuf_header {};
    msgpack::pack(sbuf_header, header_);
    frames.addmem(sbuf_header.data(), sbuf_header.size());

    // Second frame until Nth frame: payload
    for(auto& frame : payload_frames_) {
        if(!frame->empty()) {
            zmq::message_t new_frame {};
            new_frame.swap(*frame);
            frames.add(std::move(new_frame));
        }
    }
    // clear payload_frames_ member as payload has been swapped
    payload_frames_.clear();
    return frames;
}

CDTP1Message CDTP1Message::disassemble(zmq::multipart_t& frames) {
    if(frames.size() < 2) {
        // TODO(simonspa): throw
    }

    auto frame_it = frames.begin();

    // Decode header
    const auto header = Header::disassemble({to_byte_ptr(frame_it->data()), frame_it->size()});
    std::advance(frame_it, 1);

    // Create message, reversing space for frames
    auto cdtp_message = CDTP1Message(header, frames.size() - 1);

    // Swap payload
    for(; frame_it != frames.end(); frame_it++) {
        auto new_frame = std::make_shared<zmq::message_t>();
        new_frame->swap(*frame_it);
        cdtp_message.addPayload(std::move(new_frame));
    }

    return cdtp_message;
}
