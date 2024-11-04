/**
 * @file
 * @brief Implementation of Message Header
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "BaseHeader.hpp"

#include <chrono>
#include <cstddef>
#include <ios>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <msgpack.hpp>

#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/protocol/Protocol.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::utils;
using namespace constellation::protocol;
using namespace std::string_view_literals;

// Similar to CDTP1Header::disassemble in CDTP1Header.cpp, check when modifying
BaseHeader BaseHeader::disassemble(Protocol protocol, std::span<const std::byte> data) {
    try {
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

        if(protocol_recv != protocol) {
            throw UnexpectedProtocolError(protocol_recv, protocol);
        }

        // Unpack sender
        const auto msgpack_sender = msgpack::unpack(to_char_ptr(data.data()), data.size_bytes(), offset);
        const auto sender = msgpack_sender->as<std::string>();

        // Unpack time
        const auto msgpack_time = msgpack::unpack(to_char_ptr(data.data()), data.size_bytes(), offset);
        const auto time = msgpack_time->as<std::chrono::system_clock::time_point>();

        // Unpack tags
        const auto msgpack_tags = msgpack::unpack(to_char_ptr(data.data()), data.size_bytes(), offset);
        const auto tags = msgpack_tags->as<Dictionary>();

        // Construct header
        return {protocol, sender, time, tags};
    } catch(const msgpack::type_error&) {
        throw MessageDecodingError("malformed data");
    } catch(const msgpack::unpack_error&) {
        throw MessageDecodingError("could not unpack data");
    }
}

void BaseHeader::msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const {
    // first pack version
    msgpack_packer.pack(get_protocol_identifier(protocol_));
    // then sender
    msgpack_packer.pack(sender_);
    // then time
    msgpack_packer.pack(time_);
    // then tags
    msgpack_packer.pack(tags_);
}

std::string BaseHeader::to_string() const {
    std::ostringstream out {};
    std::boolalpha(out);
    out << "Header: "sv << get_readable_protocol(protocol_) << '\n' //
        << "Sender: "sv << sender_ << '\n'                          //
        << "Time:   "sv << utils::to_string(time_) << '\n'          //
        << "Tags:"sv << tags_.to_string();

    return out.str();
}
