/**
 * @file
 * @brief Implementation of the CHIRP message
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "CHIRPMessage.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/external/md5.h"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/utils/networking.hpp"
#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::chirp;
using namespace constellation::message;

MD5Hash::MD5Hash(std::string_view string) : array() {
    auto hasher = Chocobo1::MD5();
    hasher.addData(string.data(), string.length());
    hasher.finalize();
    // Swap output of hash with this to avoid copy
    auto hash_copy = hasher.toArray();
    this->swap(hash_copy);
}

std::string MD5Hash::to_string() const {
    std::string ret {};
    // Resize string to twice the hash length as two character needed per byte
    ret.resize(2 * this->size());
    for(std::size_t n = 0; n < this->size(); ++n) {
        // First character of byte hex representation
        const auto hash_upper = (this->at(n) & 0xF0U) >> 4U;
        ret.at(2 * n) = static_cast<char>(hash_upper < 10 ? hash_upper + '0' : hash_upper - 10 + 'a');
        // Second character of byte hex representation
        const auto hash_lower = this->at(n) & 0x0FU;
        ret.at((2 * n) + 1) = static_cast<char>(hash_lower < 10 ? hash_lower + '0' : hash_lower - 10 + 'a');
    }
    return ret;
}

bool MD5Hash::operator<(const MD5Hash& other) const {
    for(std::size_t n = 0; n < this->size(); ++n) {
        if(this->at(n) < other.at(n)) {
            return true;
        }
    }
    return false;
}

CHIRPMessage::CHIRPMessage(
    MessageType type, MD5Hash group_id, MD5Hash host_id, ServiceIdentifier service_id, utils::Port port)
    : type_(type), group_id_(group_id), host_id_(host_id), service_id_(service_id), port_(port) {}

CHIRPMessage::CHIRPMessage(
    MessageType type, std::string_view group, std::string_view host, ServiceIdentifier service_id, utils::Port port)
    : CHIRPMessage(type, MD5Hash(group), MD5Hash(host), service_id, port) {}

AssembledMessage CHIRPMessage::assemble() const {
    AssembledMessage ret {};

    // Protocol identifier
    for(std::size_t n = 0; n < CHIRP_IDENTIFIER.length(); ++n) {
        ret.at(n) = std::byte(CHIRP_IDENTIFIER.at(n));
    }
    // Protocol version
    ret.at(5) = std::byte(CHIRP_VERSION);
    // Message Type
    ret.at(6) = std::byte(std::to_underlying(type_));
    // Group Hash
    for(std::uint8_t n = 0; n < 16; ++n) {
        ret.at(7 + n) = std::byte(group_id_.at(n));
    }
    // Host Hash
    for(std::uint8_t n = 0; n < 16; ++n) {
        ret.at(23 + n) = std::byte(host_id_.at(n));
    }
    // Service Identifier
    ret.at(39) = std::byte(std::to_underlying(service_id_));
    // Port in network byte order (MSB first)
    ret.at(40) = std::byte(static_cast<std::uint8_t>(static_cast<unsigned int>(port_ >> 8U) & 0x00FFU));
    ret.at(41) = std::byte(static_cast<std::uint8_t>(port_ & 0x00FFU));

    return ret;
}

CHIRPMessage CHIRPMessage::disassemble(std::span<const std::byte> assembled_message) {
    // Create new message
    auto chirp_message = CHIRPMessage();

    // Check size
    if(assembled_message.size() != CHIRP_MESSAGE_LENGTH) {
        throw MessageDecodingError("message length is not " + utils::to_string(CHIRP_MESSAGE_LENGTH) + " bytes");
    }
    // Check protocol identifier
    for(std::size_t n = 0; n < CHIRP_IDENTIFIER.length(); ++n) {
        if(std::to_integer<char>(assembled_message[n]) != CHIRP_IDENTIFIER.at(n)) {
            throw MessageDecodingError("not a CHIRP broadcast");
        }
    }
    // Check the protocol version
    if(std::to_integer<std::uint8_t>(assembled_message[5]) != CHIRP_VERSION) {
        throw MessageDecodingError("not a CHIRP v1 broadcast");
    }
    // Message Type
    if(std::to_integer<std::uint8_t>(assembled_message[6]) < std::to_underlying(REQUEST) ||
       std::to_integer<std::uint8_t>(assembled_message[6]) > std::to_underlying(DEPART)) {
        throw MessageDecodingError("message type invalid");
    }
    chirp_message.type_ = static_cast<MessageType>(assembled_message[6]);
    // Group ID
    for(std::uint8_t n = 0; n < 16; ++n) {
        chirp_message.group_id_.at(n) = std::to_integer<std::uint8_t>(assembled_message[7 + n]);
    }
    // Host ID
    for(std::uint8_t n = 0; n < 16; ++n) {
        chirp_message.host_id_.at(n) = std::to_integer<std::uint8_t>(assembled_message[23 + n]);
    }
    // Service Identifier
    if(std::to_integer<std::uint8_t>(assembled_message[39]) < std::to_underlying(CONTROL) ||
       std::to_integer<std::uint8_t>(assembled_message[39]) > std::to_underlying(DATA)) {
        throw MessageDecodingError("service identifier invalid");
    }
    chirp_message.service_id_ = static_cast<ServiceIdentifier>(assembled_message[39]);
    // Port from network byte order (MSB first)
    chirp_message.port_ = static_cast<std::uint16_t>(std::to_integer<unsigned int>(assembled_message[40]) << 8U) +
                          std::to_integer<std::uint8_t>(assembled_message[41]);

    return chirp_message;
}
