/**
 * @file
 * @brief Utilities for manipulating strings
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cctype>
#include <concepts>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

#include <magic_enum.hpp>

namespace constellation::utils {

    template <typename T> inline std::string transform(std::string_view string, const T& operation) {
        std::string out {};
        out.reserve(string.size());
        for(auto character : string) {
            out += static_cast<char>(operation(static_cast<unsigned char>(character)));
        }
        return out;
    }

    template <typename R>
        requires std::ranges::range<R> && std::convertible_to<std::ranges::range_value_t<R>, std::string_view>
    inline std::string list_strings(R strings) {
        std::string out {};
        for(std::string_view string : strings) {
            out += string;
            out += ", ";
        }
        // Remove last ", "
        out.erase(out.size() - 2);
        return out;
    }

    template <typename E>
        requires std::is_enum_v<E>
    inline std::string list_enum_names() {
        constexpr auto enum_names = magic_enum::enum_names<E>();
        return list_strings(enum_names);
    }

} // namespace constellation::utils
