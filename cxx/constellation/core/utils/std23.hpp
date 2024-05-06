/**
 * @file
 * @brief C++23 library features for C++20
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <version>

// NOLINTBEGIN(cert-dcl58-cpp)

// std::to_underlying
#ifndef __cpp_lib_to_underlying
#include <type_traits>
namespace std {
    template <typename E> constexpr typename std::underlying_type<E>::type to_underlying(E e) noexcept {
        return static_cast<typename std::underlying_type<E>::type>(e);
    }
} // namespace std
#endif

// std::unreachable
#ifndef __cpp_lib_unreachable
namespace std {
    [[noreturn]] inline void unreachable() {
#ifdef __GNUC__
        __builtin_unreachable();
#endif
    }
} // namespace std
#endif

// NOLINTEND(cert-dcl58-cpp)
