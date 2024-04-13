/**
 * @file
 * @brief Tags for type dispatching and run time type identification
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdlib>
#include <cxxabi.h>
#include <memory>
#include <source_location>
#include <string>

namespace constellation::utils {

    /** Helpers for compile-time demangling using source_location */
    struct dummy_type {};
    template <typename T> auto embed_type() {
        return std::string_view {std::source_location::current().function_name()};
    }

    /**
     * @brief Demangle the type to human-readable form if it is mangled
     * @param type Type info of the mangled name
     */
    template <typename T> inline std::string_view demangle(const T& = {}) {
        auto dummy_sig = embed_type<dummy_type>();
        auto start = dummy_sig.find("dummy_type");
        auto embed_sig = embed_type<T>();
        auto type_length = embed_sig.size() - dummy_sig.size() + 10;
        return embed_sig.substr(start, type_length);
    }

} // namespace constellation::utils
