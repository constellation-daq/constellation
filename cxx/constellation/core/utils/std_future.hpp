/**
 * @file
 * @brief Future C++ library features for C++20 and newer on older compilers
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

// IWYU pragma: always_keep

#pragma once

#include <version>

// NOLINTBEGIN(cert-dcl58-cpp,readability-duplicate-include)

// std::to_underlying
#ifndef __cpp_lib_to_underlying

#include <type_traits>

namespace std {
    template <typename E> constexpr typename std::underlying_type_t<E> to_underlying(E e) noexcept {
        return static_cast<typename std::underlying_type_t<E>>(e);
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

// std::ranges::constant_range
#ifndef __cpp_lib_ranges_as_const

#include <concepts>
#include <ranges>
#include <utility>

namespace std::ranges {
    template <typename R>
    concept constant_range = std::ranges::input_range<R> &&
                             std::same_as<std::ranges::iterator_t<R>, decltype(std::ranges::cbegin(std::declval<R&>()))>;
} // namespace std::ranges

#endif

// std::ranges::to
#ifndef __cpp_lib_ranges_to_container

#include <concepts>
#include <ranges>
#include <utility>

namespace std::ranges {
    template <template <typename...> typename C, typename R, typename... Args>
        requires std::ranges::input_range<R> && std::constructible_from<C<std::ranges::range_value_t<R>>,
                                                                        std::ranges::iterator_t<R>,
                                                                        std::ranges::sentinel_t<R>,
                                                                        Args...>
    C<std::ranges::range_value_t<R>> to(R range, Args&&... args) {
        return {std::ranges::begin(range), std::ranges::end(range), std::forward<Args>(args)...};
    }

} // namespace std::ranges

#endif

// NOLINTEND(cert-dcl58-cpp,readability-duplicate-include)
