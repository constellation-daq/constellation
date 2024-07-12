/**
 * @file
 * @brief Implementation of the CHIRP message
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "CHIRP2Message.hpp"

#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <magic_enum.hpp>
#include <msgpack.hpp>

#include "constellation/core/chirp/CHIRPService.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/message/Protocol.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/networking.hpp"
#include "constellation/core/utils/std_future.hpp"

using namespace constellation::chirp;
using namespace constellation::message;
using namespace constellation::utils;

using enum CHIRP2Message::Type;
using enum CHIRPService::Identifier;

CHIRP2Message::CHIRP2Message(
    std::string group_name, std::string host_name, Type type, CHIRPService::Identifier service_identifier, Port port)
    : group_name_(std::move(group_name)), host_name_(std::move(host_name)), type_(type),
      service_identifier_(service_identifier), port_(port) {
    // Throw if service identifier is ANY but not REQUEST
    if(service_identifier_ == ANY && type != REQUEST) {
        // TODO throw
    }
}

PayloadBuffer CHIRP2Message::assemble() const {
    msgpack::sbuffer sbuf {};

    // first pack protocol
    msgpack::pack(sbuf, get_protocol_identifier(CHIRP2));
    // then group name
    msgpack::pack(sbuf, group_name_);
    // then host name
    msgpack::pack(sbuf, host_name_);
    // then type
    msgpack::pack(sbuf, std::to_underlying(type_));
    // then service identifier
    msgpack::pack(sbuf, std::to_underlying(service_identifier_));
    // then port
    msgpack::pack(sbuf, port_);

    return {std::move(sbuf)};
}

CHIRP2Message CHIRP2Message::disassemble(std::span<const std::byte> bytes) {
    try {
        // Offset since we decode separate msgpack objects
        std::size_t offset = 0;

        // Unpack protocol
        const auto msgpack_protocol_identifier = msgpack::unpack(to_char_ptr(bytes.data()), bytes.size(), offset);
        const auto protocol_identifier = msgpack_protocol_identifier->as<std::string>();

        // Try to decode protocol identifier into protocol
        Protocol protocol_recv {};
        try {
            protocol_recv = get_protocol(protocol_identifier);
        } catch(const std::invalid_argument&) {
            throw InvalidProtocolError(protocol_identifier);
        }
        if(protocol_recv != CHIRP2) {
            throw UnexpectedProtocolError(protocol_recv, CHIRP2);
        }

        // Unpack CHIRP group name
        const auto msgpack_group_name = msgpack::unpack(to_char_ptr(bytes.data()), bytes.size(), offset);
        const auto group_name = msgpack_group_name->as<std::string>();

        // Unpack host name
        const auto msgpack_host_name = msgpack::unpack(to_char_ptr(bytes.data()), bytes.size(), offset);
        const auto host_name = msgpack_host_name->as<std::string>();

        // Unpack CHIRP message type
        const auto msgpack_type = msgpack::unpack(to_char_ptr(bytes.data()), bytes.size(), offset);
        const auto type = static_cast<Type>(msgpack_type->as<std::underlying_type_t<Type>>());

        // Check that CHIRP message type is valid
        if(!magic_enum::enum_contains(type)) {
            throw MessageDecodingError("invalid message type");
        }

        // Unpack service identifier
        const auto msgpack_service_identifier = msgpack::unpack(to_char_ptr(bytes.data()), bytes.size(), offset);
        const auto service_identifier =
            static_cast<CHIRPService::Identifier>(msgpack_type->as<std::underlying_type_t<CHIRPService::Identifier>>());

        // Check that service identifier is valid
        if(!magic_enum::enum_contains(service_identifier)) {
            throw MessageDecodingError("invalid service identifier");
        }

        // Check that service identifier is not ANY if not REQUEST message
        if(service_identifier == ANY && type != REQUEST) {
            throw MessageDecodingError("service identifier can only be ANY in REQUEST messages");
        }

        // Unpack port
        const auto msgpack_port = msgpack::unpack(to_char_ptr(bytes.data()), bytes.size(), offset);
        const auto port = msgpack_port->as<utils::Port>();

        // Construct message
        return {group_name, host_name, type, service_identifier, port};

    } catch(const msgpack::type_error&) {
        throw MessageDecodingError("malformed data");
    } catch(const msgpack::unpack_error&) {
        throw MessageDecodingError("could not unpack data");
    }
}
