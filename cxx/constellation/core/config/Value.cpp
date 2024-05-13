/**
 * @file
 * @brief Implementation of Dictionary
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Value.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <span>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <variant>
#include <vector>

#include <msgpack.hpp>

#include "constellation/core/utils/string.hpp"

using namespace constellation::config;
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
                auto char_as_hex = [](char c) -> std::string {
                    std::string hex {"00"};
                    auto* last = hex.data() + hex.size();
                    auto res = std::to_chars(hex.data(), last, static_cast<std::uint8_t>(c), 16);
                    if(res.ptr != last) {
                        // c<16, i.e. written to first char of hex -> reverse string
                        std::reverse(hex.begin(), hex.end());
                    }
                    return "0x" + transform(hex, ::toupper);
                };
                out = "[ " + list_to_string(arg, char_as_hex, " ") + " ]";
            } else if constexpr(convertible_list_to_string<T>) {
                out = "[" + list_to_string(arg) + "]";
            }
            return out;
        },
        *this);
}

const std::type_info& Value::type() const {
    return std::visit([](auto&& x) -> decltype(auto) { return typeid(x); }, *this);
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
