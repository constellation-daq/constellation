/**
 * @file
 * @brief Dictionary type with serialization functions for MessagePack
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include <msgpack/object_decl.hpp>
#include <msgpack/pack_decl.hpp>
#include <msgpack/sbuffer_decl.hpp>

#include "constellation/core/config.hpp"

namespace constellation::config {

    /** Check if a type can be held by a variant */
    template <class T, class U> struct is_one_of;
    template <class T, class... Ts>
    struct is_one_of<T, std::variant<Ts...>> : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};

    /**
     * Value type for Dictionary using std::variant
     *
     * Allowed types: nil, bool, long int, double, string, time point and vectors thereof
     */
    using value_t = std::variant<std::monostate,
                                 bool,
                                 std::int64_t,
                                 double,
                                 std::string,
                                 std::chrono::system_clock::time_point,
                                 std::vector<bool>,
                                 std::vector<std::int64_t>,
                                 std::vector<double>,
                                 std::vector<std::string>,
                                 std::vector<std::chrono::system_clock::time_point>>;

    /**
     * @class Value
     * @brief Augmented std::variant with MsgPack packer and unpacker routines
     */
    class CNSTLN_API Value : public value_t {
    public:
        using value_t::value_t;
        using value_t::operator=;

        /**
         * @brief Convert value to string representation
         * @return String representation of the value
         */
        std::string str() const;

        /**
         * @brief Get type info of the value currently stored in the variant
         * @return Type info of the currently held value
         */
        const std::type_info& type() const;

        /** Pack value with msgpack */
        CNSTLN_API void msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const;

        /** Unpack value with msgpack */
        CNSTLN_API void msgpack_unpack(const msgpack::object& msgpack_object);
    };

    /**
     * List type with serialization functions for MessagePack
     */
    class List final : public std::vector<Value> {
    public:
        /** Pack list with msgpack */
        CNSTLN_API void msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const;

        /** Unpack list with msgpack */
        CNSTLN_API void msgpack_unpack(const msgpack::object& msgpack_object);
    };

    /**
     * Dictionary type with serialization functions for MessagePack
     */
    class Dictionary final : public std::map<std::string, Value> {
    public:
        /** Pack dictionary with msgpack */
        CNSTLN_API void msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const;

        /** Unpack dictionary with msgpack */
        CNSTLN_API void msgpack_unpack(const msgpack::object& msgpack_object);
    };

} // namespace constellation::config
