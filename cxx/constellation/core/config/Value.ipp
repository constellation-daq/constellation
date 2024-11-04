/**
 * @file
 * @brief Dictionary type with serialization functions for MessagePack
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "Value.hpp" // NOLINT(misc-header-include-cycle)

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <magic_enum.hpp>

#include "constellation/core/utils/string.hpp"
#include "constellation/core/utils/type.hpp"

namespace constellation::config {

    template <typename T> T Value::get() const {

        // When asking for a vector but we get NIL, let's return an empty vector of the right type
        // since we also set std::monostate when encountering an empty msgpack array
        if(utils::is_std_vector_v<T> && std::holds_alternative<std::monostate>(*this)) {
            return {};
        }

        // Value is directly held by variant
        if constexpr(is_one_of_v<T, value_t>) {
            return std::get<T>(*this);

        } else if constexpr(std::is_arithmetic_v<T>) {
            if(std::holds_alternative<std::int64_t>(*this)) {
                return static_cast<T>(std::get<std::int64_t>(*this));
            }
            if(std::holds_alternative<double>(*this)) {
                return static_cast<T>(std::get<double>(*this));
            }

        } else if constexpr(std::is_enum_v<T>) {
            const auto& str = std::get<std::string>(*this);
            const auto enum_val = magic_enum::enum_cast<T>(str, magic_enum::case_insensitive);

            if(!enum_val.has_value()) {
                throw std::invalid_argument("possible values are " + utils::list_enum_names<T>());
            }
            return enum_val.value();

        } else if constexpr(utils::is_std_vector_v<T>) {
            using U = typename T::value_type;

            if constexpr(std::is_arithmetic_v<U>) {
                if(std::holds_alternative<std::vector<std::int64_t>>(*this)) {
                    const auto& vec = std::get<std::vector<std::int64_t>>(*this);
                    return T(vec.begin(), vec.end());
                }
                if(std::holds_alternative<std::vector<double>>(*this)) {
                    const auto& vec = std::get<std::vector<double>>(*this);
                    return T(vec.begin(), vec.end());
                }

            } else if constexpr(std::is_enum_v<U>) {
                const auto& vec = std::get<std::vector<std::string>>(*this);
                T result {};
                result.reserve(vec.size());

                std::for_each(vec.begin(), vec.end(), [&](const auto& str) {
                    const auto enum_val = magic_enum::enum_cast<T>(str, magic_enum::case_insensitive);
                    if(!enum_val.has_value()) {
                        throw std::invalid_argument("possible values are " + utils::list_enum_names<T>());
                    }
                    result.emplace_back(enum_val.value());
                });
                return result;
            }
        }

        throw std::bad_variant_access();
    }

    template <typename T> Value Value::set(const T& val) {
        if constexpr(std::is_same_v<T, Value>) {
            return val;

        } else if constexpr(is_one_of_v<T, value_t>) {
            return {val};

        } else if constexpr(is_bounded_type_array<char, T> || std::is_same_v<std::string_view, T>) {
            // special case for char-array constructed strings and string_view
            return {std::string(val)};

        } else if constexpr(std::is_integral_v<T>) {
            if(!std::in_range<std::int64_t>(val)) {
                throw std::overflow_error("type overflow");
            }
            return {static_cast<std::int64_t>(val)};

        } else if constexpr(std::is_floating_point_v<T>) {
            return {static_cast<double>(val)};

        } else if constexpr(std::is_enum_v<T>) {
            return {utils::to_string(val)};

        } else if constexpr(utils::is_std_vector_v<T>) {
            using U = typename T::value_type;

            if constexpr(std::is_integral_v<U>) {
                std::vector<std::int64_t> nval {};
                nval.reserve(val.size());
                for(auto val_elem : val) {
                    if(!std::in_range<std::int64_t>(val_elem)) {
                        throw std::overflow_error("type overflow");
                    }
                    nval.emplace_back(static_cast<std::int64_t>(val_elem));
                }
                return {nval};

            } else if constexpr(std::is_floating_point_v<U>) {
                // Note: since std::vector<double> is part of variant, this is essentially only std::vector<float>
                const std::vector<double> nval {val.begin(), val.end()};
                return {nval};

            } else if constexpr(std::is_enum_v<U>) {
                std::vector<std::string> nval {};
                nval.reserve(val.size());

                std::for_each(
                    val.begin(), val.end(), [&](const auto& enum_val) { nval.emplace_back(utils::to_string(enum_val)); });
                return {nval};
            }
        }

        throw std::bad_cast();
    }

} // namespace constellation::config
