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
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include <msgpack/object_decl.hpp>
#include <msgpack/pack_decl.hpp>
#include <msgpack/sbuffer_decl.hpp>

#include "constellation/build.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/utils/type.hpp"

namespace constellation::config {

    /// @cond doxygen_suppress

    // Check if a type can be held by a variant
    template <typename T, typename U> struct is_one_of : std::false_type {};
    template <typename T, typename... Ts>
    struct is_one_of<T, std::variant<Ts...>> : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};
    template <typename T, typename... Ts> inline constexpr bool is_one_of_v = is_one_of<T, Ts...>::value;

    // Concept for bounded C arrays of given type U
    template <typename U, typename T>
    concept is_bounded_type_array = std::is_bounded_array_v<T> && std::is_same_v<std::remove_extent_t<T>, U>;

    /// @endcond

    /**
     * Value type for Dictionary using std::variant
     *
     * Allowed types: nil, bool, int64, double, string, time point, vectors of bool, int64, double, string, time point,
     *                bytes (vector of char)
     */
    using value_t = std::variant<std::monostate,
                                 bool,
                                 std::int64_t,
                                 double,
                                 std::string,
                                 std::chrono::system_clock::time_point,
                                 std::vector<bool>,
                                 std::vector<char>,
                                 std::vector<std::int64_t>,
                                 std::vector<double>,
                                 std::vector<std::string>,
                                 std::vector<std::chrono::system_clock::time_point>>;

    /**
     * @class Value
     * @brief Augmented std::variant with MsgPack packer and unpacker routines
     */
    class Value : public value_t {
    public:
        using value_t::value_t;
        using value_t::operator=;

        /**
         * @brief Convert value to string representation
         * @return String representation of the value
         */
        CNSTLN_API std::string str() const;

        /**
         * @brief Get value in requested type
         * @return Value in the type of the requested template parameter
         *
         * @throws invalid_argument If the conversion to the requested type did not succeed
         * @throws bad_variant_access If no suitable conversion was found and direct access did not succeed
         */
        template <typename T> T get() const;

        /**
         * @brief Set value from provided type
         * @return Value
         *
         * @throws std::invalid_argument If the conversion from the provided type did not succeed
         * @throws std::bad_variant_access If no suitable conversion was found and direct assignment did not succeed
         */
        template <typename T> static inline Value set(const T& value);

        /**
         * @brief Demangle type held by the Value
         * @return Demangled name of the currently held type
         */
        std::string demangle() const {
            return std::visit([&](auto&& arg) { return utils::demangle<std::decay_t<decltype(arg)>>(); }, *this);
        }

        /** Pack value with msgpack */
        CNSTLN_API void msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const;

        /** Unpack value with msgpack */
        CNSTLN_API void msgpack_unpack(const msgpack::object& msgpack_object);

        /** Assemble list via msgpack to message payload */
        CNSTLN_API message::PayloadBuffer assemble() const;

        /** Disassemble list from message payload */
        CNSTLN_API static Value disassemble(const message::PayloadBuffer& message);
    };
} // namespace constellation::config

// Include template members
#include "Value.ipp" // IWYU pragma: keep
