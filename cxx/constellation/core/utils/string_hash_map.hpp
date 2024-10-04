/**
 * @file
 * @brief Unordered string map using hashes for fast lookup
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdlib>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace constellation::utils {

    // NOLINTBEGIN(readability-identifier-naming)

    /** Hash for std::unorderd_map */
    struct string_hash {
        using hash_type = std::hash<std::string_view>;
        using is_transparent = void;

        [[nodiscard]] std::size_t operator()(const char* str) const { return hash_type {}(str); }
        [[nodiscard]] std::size_t operator()(std::string_view str) const { return hash_type {}(str); }
        [[nodiscard]] std::size_t operator()(const std::string& str) const { return hash_type {}(str); }
    };

    /** Unordered string map using hashes for fast lookup  */
    template <typename V> using string_hash_map = std::unordered_map<std::string, V, string_hash, std::equal_to<>>;

    // NOLINTEND(readability-identifier-naming)

} // namespace constellation::utils
