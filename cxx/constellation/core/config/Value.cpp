/**
 * @file
 * @brief Implementation of Dictionary
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Value.hpp"

#include <chrono>
#include <concepts>
#include <cstdint>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <msgpack.hpp>

#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::utils;

std::string Value::str() const {
    return std::visit(
        [](auto&& arg) -> std::string {
            std::string out;
            using T = std::decay_t<decltype(arg)>;
            if constexpr(std::same_as<T, std::monostate>) {
                out = "NIL";
            } else if constexpr(convertible_to_string<T>) {
                out = to_string(arg);
            } else if constexpr(std::same_as<T, std::vector<char>>) {
                // Special case: print chars in hex
                out = "[ " + range_to_string(arg, char_to_hex_string, " ") + " ]";
            } else if constexpr(convertible_range_to_string<T>) {
                out = "[" + range_to_string(arg) + "]";
            }
            return out;
        },
        *this);
}

void Value::msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const {
    std::visit(
        [&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr(std::same_as<T, std::monostate>) {
                // std::monostate => nil
                msgpack_packer.pack_nil();
            } else {
                msgpack_packer.pack(arg);
            }
        },
        *this);
}

void Value::msgpack_unpack(const msgpack::object& msgpack_object) {

    // Check for arrays - we decode them in one go to ensure same-type values
    if(msgpack_object.type == msgpack::type::ARRAY) {
        const auto msgpack_array_raw = msgpack_object.via.array; // NOLINT(cppcoreguidelines-pro-type-union-access)
        const auto msgpack_array = std::span(msgpack_array_raw.ptr, msgpack_array_raw.size);

        // If empty we only store nil:
        if(msgpack_array.empty()) {
            *this = std::monostate();
            return;
        }

        switch(msgpack_array.front().type) {
        case msgpack::type::BOOLEAN: {
            *this = msgpack_object.as<std::vector<bool>>();
            break;
        }
        case msgpack::type::POSITIVE_INTEGER:
        case msgpack::type::NEGATIVE_INTEGER: {
            *this = msgpack_object.as<std::vector<std::int64_t>>();
            break;
        }
        case msgpack::type::FLOAT32:
        case msgpack::type::FLOAT64: {
            *this = msgpack_object.as<std::vector<double>>();
            break;
        }
        case msgpack::type::STR: {
            *this = msgpack_object.as<std::vector<std::string>>();
            break;
        }
        case msgpack::type::EXT: {
            // Try to convert to time_point, throws if wrong EXT type
            *this = msgpack_object.as<std::vector<std::chrono::system_clock::time_point>>();
            break;
        }
        default: {
            throw msgpack::type_error();
        }
        }

    } else {
        switch(msgpack_object.type) {
        case msgpack::type::BOOLEAN: {
            *this = msgpack_object.as<bool>();
            break;
        }
        case msgpack::type::POSITIVE_INTEGER:
        case msgpack::type::NEGATIVE_INTEGER: {
            *this = msgpack_object.as<std::int64_t>();
            break;
        }
        case msgpack::type::FLOAT32:
        case msgpack::type::FLOAT64: {
            *this = msgpack_object.as<double>();
            break;
        }
        case msgpack::type::BIN: {
            *this = msgpack_object.as<std::vector<char>>();
            break;
        }
        case msgpack::type::STR: {
            *this = msgpack_object.as<std::string>();
            break;
        }
        case msgpack::type::EXT: {
            // Try to convert to time_point, throws if wrong EXT type
            *this = msgpack_object.as<std::chrono::system_clock::time_point>();
            break;
        }
        case msgpack::type::NIL: {
            *this = std::monostate();
            break;
        }
        default: {
            throw msgpack::type_error();
        }
        }
    }
}

PayloadBuffer Value::assemble() const {
    msgpack::sbuffer sbuf {};
    msgpack::pack(sbuf, *this);
    return {std::move(sbuf)};
}

Value Value::disassemble(const PayloadBuffer& message) {
    const auto msgpack_dict = msgpack::unpack(to_char_ptr(message.span().data()), message.span().size());
    return msgpack_dict->as<Value>();
}
