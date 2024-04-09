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

    template <typename E>
        requires std::is_enum_v<E>
    inline std::string list_enum_names() {
        std::string out {};
        constexpr auto enum_names = magic_enum::enum_names<E>();
        for(const auto enum_name : enum_names) {
            out += transform(enum_name, ::tolower) + ", ";
        }
        // Remove last ", "
        out.erase(out.size() - 2);
        return out;
    }

} // namespace constellation::utils
