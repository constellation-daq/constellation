/**
 * @file
 * @brief Future C++ library features for C++20 and older compilers
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

// stream formatters for std::chrono
#ifndef __cpp_lib_format
#include <chrono>
#include <ostream>
#include "constellation/core/utils/string.hpp"
namespace std {
    inline std::ostream& operator<<(std::ostream& stream, std::chrono::system_clock::time_point time_point) {
        return stream << constellation::utils::to_string(time_point);
    }
    template <typename Rep, typename Period>
    inline std::ostream& operator<<(std::ostream& stream, std::chrono::duration<Rep, Period> duration) {
        return stream << constellation::utils::to_string(duration);
    }
} // namespace std
#endif

// NOLINTEND(cert-dcl58-cpp)
