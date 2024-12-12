/**
 * @file
 * @brief Enums functions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <concepts>
#include <ios>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#if __has_include(<magic_enum/magic_enum.hpp>)
#include <magic_enum/magic_enum.hpp>
#include <magic_enum/magic_enum_flags.hpp>
#else
#include <magic_enum.hpp>
#include <magic_enum_flags.hpp>
#endif

namespace constellation::utils {

    template <typename E>
        requires std::is_enum_v<E>
    constexpr auto enum_cast(std::underlying_type_t<E> value) noexcept {
        return magic_enum::enum_cast<E>(value);
    }

    template <typename E>
        requires std::is_enum_v<E>
    constexpr auto enum_cast(std::string_view value, bool case_insensitive = true) noexcept {
        std::optional<E> retval {};
        retval = case_insensitive ? magic_enum::enum_cast<E>(value, magic_enum::case_insensitive)
                                  : magic_enum::enum_cast<E>(value);
        // If unscoped enum and no value, try cast as flag
        if constexpr(magic_enum::is_unscoped_enum_v<E>) {
            if(!retval.has_value()) {
                retval = case_insensitive ? magic_enum::enum_flags_cast<E>(value, magic_enum::case_insensitive)
                                          : magic_enum::enum_flags_cast<E>(value);
            }
        }
        return retval;
    }

    template <typename E>
        requires std::is_enum_v<E>
    auto enum_name(E enum_val) noexcept {
        auto retval = std::string(magic_enum::enum_name<E>(enum_val));
        // If unscoped enum and no value, try as flag
        if constexpr(magic_enum::is_unscoped_enum_v<E>) {
            if(retval.empty()) {
                retval = magic_enum::enum_flags_name<E>(enum_val);
            }
        }
        return retval;
    }

    template <typename E>
        requires std::is_enum_v<E>
    constexpr auto enum_names() noexcept {
        return magic_enum::enum_names<E>();
    }

} // namespace constellation::utils

// Bitwise operators for enums
using namespace magic_enum::bitwise_operators; // NOLINT(google-global-names-in-headers)

// Stream operator<< for enums
template <typename S, typename E>
    requires std::derived_from<std::remove_cvref_t<S>, std::ios_base> && std::is_enum_v<E>
inline S&& operator<<(S&& os, E value) {
    os << constellation::utils::enum_name(value);
    return std::forward<S>(os);
}

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

// Adjust enum range and specify whether to use as flags
#define ENUM_SET_RANGE(ENUM, MIN, MAX)                                                                                      \
    template <> struct magic_enum::customize::enum_range<ENUM> {                                                            \
        static constexpr int min = MIN;                                                                                     \
        static constexpr int max = MAX;                                                                                     \
    }

// NOLINTEND(cppcoreguidelines-macro-usage)
