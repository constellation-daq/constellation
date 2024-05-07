/**
 * @file
 * @brief Utilities for manipulating strings
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <algorithm>
#include <cctype>
#include <concepts>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>

#include <magic_enum.hpp>

#include "constellation/core/utils/chrono.hpp"

namespace constellation::utils {

    template <typename T> inline std::string transform(std::string_view string, const T& operation) {
        std::string out {};
        out.reserve(string.size());
        for(auto character : string) {
            out += static_cast<char>(operation(static_cast<unsigned char>(character)));
        }
        return out;
    }

    template <typename T>
        requires std::convertible_to<T, std::string_view>
    inline std::string to_string(T string_like) {
        const std::string_view string_view {string_like};
        return {string_view.data(), string_view.size()};
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    inline std::string to_string(T number) {
        if constexpr(std::same_as<T, bool>) {
            return number ? "true" : "false";
        }
        return std::to_string(number);
    }

    template <typename T>
        requires chrono_time_point<T>
    std::string to_string(T tp) {
        return time_point_to_string(tp);
    }

    template <typename E>
        requires std::is_enum_v<E>
    inline std::string to_string(E enum_val) {
        return to_string(magic_enum::enum_name<E>(enum_val));
    }

    template <typename T>
    concept convertible_to_string = requires(T t) {
        { to_string(t) } -> std::same_as<std::string>;
    };

    template <typename R, typename F>
        requires std::ranges::range<R> && std::is_invocable_r_v<std::string, F, std::ranges::range_value_t<R>>
    inline std::string list_to_string(const R& range, F to_string_func, const std::string& delim = ", ") {
        std::string out {};
        if(!std::ranges::empty(range)) {
            std::ranges::for_each(std::ranges::subrange(std::cbegin(range), std::ranges::prev(std::ranges::cend(range))),
                                  [&](const auto& element) { out += to_string_func(element) + delim; });
            out += to_string_func(*std::ranges::crbegin(range));
        }
        return out;
    }

    template <typename R>
        requires std::ranges::range<R> && convertible_to_string<std::ranges::range_value_t<R>>
    inline std::string list_to_string(const R& range) {
        return list_to_string(range, constellation::utils::to_string<std::ranges::range_value_t<R>>);
    }

    template <typename T>
    concept convertible_list_to_string = requires(T t) {
        { list_to_string(t) } -> std::same_as<std::string>;
    };

    template <typename E>
        requires std::is_enum_v<E>
    inline std::string list_enum_names() {
        return list_to_string(magic_enum::enum_names<E>());
    }

} // namespace constellation::utils
