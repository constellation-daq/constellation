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
#include <cctype> // IWYU pragma: export
#include <charconv>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <version> // IWYU pragma: keep

#ifdef __cpp_lib_format
#include <format>
#else
#include <ctime>
#include <iomanip>
#include <sstream>
#endif

#include <magic_enum.hpp>

namespace constellation::utils {

    /** Transforms a string with a given operation */
    template <typename F> inline std::string transform(std::string_view string, F operation) {
        std::string out {};
        out.reserve(string.size());
        for(auto character : string) {
            out += static_cast<char>(operation(static_cast<unsigned char>(character)));
        }
        return out;
    }

    /** Converts a string-like object to a string */
    template <typename S>
        requires std::convertible_to<S, std::string_view>
    inline std::string to_string(S string_like) {
        const std::string_view string_view {string_like};
        return {string_view.data(), string_view.size()};
    }

    /** Converts a bool to a string */
    template <typename B>
        requires std::same_as<B, bool>
    inline std::string to_string(B t) {
        return {t ? "true" : "false"};
    }

    /** Converts a non-boolean arithmetic object to a string */
    template <typename A>
        requires std::is_arithmetic_v<A> && (!std::same_as<A, bool>)
    inline std::string to_string(A t) {
        std::string out {};
        out.resize(25);
        const auto res = std::to_chars(out.data(), out.data() + out.size(), t);
        out.resize(res.ptr - out.data());
        return out;
    }

    /** Converts an std::chrono::system_clock::time_point to a string */
    template <typename T>
        requires std::same_as<T, std::chrono::system_clock::time_point>
    inline std::string to_string(T tp) {
#ifdef __cpp_lib_format
        return std::format("{0:%F} {0:%T}", tp);
#else
        // Convert time point to tm struct
        const auto time_t = std::chrono::system_clock::to_time_t(tp);
        std::tm tm {};
        gmtime_r(&time_t, &tm); // there is no thread-safe std::gmtime
        // Format tm as YYYY-MM-DD HH:MM:SS
        std::ostringstream oss {};
        oss << std::put_time(&tm, "%F %T");
        // Get nanoseconds since the last second
        const auto tp_in_s = std::chrono::time_point_cast<std::chrono::seconds>(tp);
        const auto ns_diff = std::chrono::time_point_cast<std::chrono::nanoseconds>(tp) -
                             std::chrono::time_point_cast<std::chrono::nanoseconds>(tp_in_s);
        oss << "." << std::setw(9) << std::setfill('0') << ns_diff.count();
        return oss.str();
#endif
    }

    /** Object that is an std::chrono::duration */
    template <typename D>
    concept is_chrono_duration = requires(D d) { std::chrono::duration(d); };

    /** Convert a duration to a string */
    template <typename D>
        requires is_chrono_duration<D>
    std::string to_string(D d) {
#ifdef __cpp_lib_format
        return std::format("{}", d);
#else
        std::string unit {};
        auto count = static_cast<double>(d.count());
        if constexpr(std::same_as<D, std::chrono::nanoseconds>) {
            unit = "ns";
        } else if constexpr(std::same_as<D, std::chrono::microseconds>) {
            unit = "us";
        } else if constexpr(std::same_as<D, std::chrono::milliseconds>) {
            unit = "ms";
        } else if constexpr(std::same_as<D, std::chrono::seconds>) {
            unit = "s";
        } else {
            // Ratio not predefined, convert to seconds
            unit = "s";
            count *= D::period::num / D::period::den;
        }
        return to_string(count) + unit;
#endif
    }

    /** Converts an enum to a string */
    template <typename E>
        requires std::is_enum_v<E>
    inline std::string to_string(E enum_val) {
        return to_string(magic_enum::enum_name<E>(enum_val));
    }

    /** Object that can be converted to a string */
    template <typename T>
    concept convertible_to_string = requires(T t) {
        { to_string(t) } -> std::same_as<std::string>;
    };

    /** Converts a range to a string with custom to_string function and delimiter */
    template <typename R, typename F>
        requires std::ranges::bidirectional_range<R> && std::is_invocable_r_v<std::string, F, std::ranges::range_value_t<R>>
    inline std::string range_to_string(const R& range, F to_string_func, const std::string& delim = ", ") {
        std::string out {};
        if(!std::ranges::empty(range)) {
            std::ranges::for_each(std::ranges::subrange(std::cbegin(range), std::ranges::prev(std::ranges::cend(range))),
                                  [&](const auto& element) { out += to_string_func(element) + delim; });
            out += to_string_func(*std::ranges::crbegin(range));
        }
        return out;
    }

    /** Converts a range to a string with custom delimiter */
    template <typename R>
        requires std::ranges::bidirectional_range<R> && convertible_to_string<std::ranges::range_value_t<R>>
    inline std::string range_to_string(const R& range, const std::string& delim = ", ") {
        return range_to_string(range, to_string<std::ranges::range_value_t<R>>, delim);
    }

    /** Range that can be converted to a string */
    template <typename T>
    concept convertible_range_to_string = requires(T t) {
        { range_to_string(t) } -> std::same_as<std::string>;
    };

    /** List all possible enum values */
    template <typename E>
        requires std::is_enum_v<E>
    inline std::string list_enum_names() {
        return range_to_string(magic_enum::enum_names<E>());
    }

    /** Convert char as hex string */
    inline std::string char_to_hex_string(char c) {
        std::string hex {"00"};
        auto* last = hex.data() + hex.size();
        auto res = std::to_chars(hex.data(), last, static_cast<std::uint8_t>(c), 16);
        if(res.ptr != last) {
            // c < 16, i.e. written to first char of hex -> reverse string
            std::ranges::reverse(hex);
        }
        return "0x" + transform(hex, ::toupper);
    }

} // namespace constellation::utils
