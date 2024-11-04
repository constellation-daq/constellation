/**
 * @file
 * @brief Tags for type dispatching and run time type identification
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <array>
#include <chrono>
#include <concepts>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <variant>
#include <vector>

#include <cxxabi.h>

#include "constellation/core/utils/string.hpp"

namespace constellation::utils {

    /// @cond doxygen_suppress

    // Type trait for std::vector
    template <typename T> struct is_std_vector : std::false_type {};
    template <typename U> struct is_std_vector<std::vector<U>> : std::true_type {};
    template <typename T> inline constexpr bool is_std_vector_v = is_std_vector<T>::value;

    // Type trait for std::array
    template <typename T> struct is_std_array : std::false_type {};
    template <typename U, std::size_t N> struct is_std_array<std::array<U, N>> : std::true_type {};
    template <typename T> inline constexpr bool is_std_array_v = is_std_array<T>::value;

    // Type trait for std::map
    template <typename T> struct is_std_map : std::false_type {};
    template <typename U, typename V> struct is_std_map<std::map<U, V>> : std::true_type {};
    template <typename T> inline constexpr bool is_std_map_v = is_std_map<T>::value;

    /// @endcond

    /**
     * @brief Demangle type to human-readable form using cxxabi
     *
     * @note This is not portable and potentially ugly, use `demangle<T>()` instead if possible.
     *
     * @param type Type info of type to demnalge (use `typeid`)
     * @return String with demangled type or mangled name if demangling failed
     */
    inline std::string demangle(const std::type_info& type) {
        int status = -1;
        const std::unique_ptr<char, void (*)(void*)> res {abi::__cxa_demangle(type.name(), nullptr, nullptr, &status),
                                                          std::free};
        if(status == 0) {
            return res.get();
        }
        // Fallback using mangled name
        return type.name();
    }

    /**
     * @brief Demangle type to human-readable form
     *
     * This function implements type demangling for common STL containers and falls back to the cxxabi `demangle()` method
     * for other STL types and non-STL types.
     *
     * @tparam T Type to demangle
     * @return String with demangled type
     */
    template <typename T> inline std::string demangle() {
        // Check if std::vector
        if constexpr(is_std_vector_v<T>) {
            using U = typename T::value_type;
            return "std::vector<" + demangle<U>() + ">";
        }
        // Check if std::array
        if constexpr(is_std_array_v<T>) {
            using U = typename T::value_type;
            return "std::array<" + demangle<U>() + ", " + to_string(std::tuple_size_v<T>) + ">";
        }
        // Check if std::map
        if constexpr(is_std_map_v<T>) {
            using U = typename T::key_type;
            using V = typename T::mapped_type;
            return "std::map<" + demangle<U>() + ", " + demangle<V>() + ">";
        }
        // Check if std::string
        if constexpr(std::same_as<T, std::string>) {
            return "std::string";
        }
        // Check if std::string_view
        if constexpr(std::same_as<T, std::string_view>) {
            return "std::string_view";
        }
        // Check if std::chrono::system_clock::time_point
        if constexpr(std::same_as<T, std::chrono::system_clock::time_point>) {
            return "std::chrono::system_clock::time_point";
        }
        // Check if std::monostate
        if constexpr(std::same_as<T, std::monostate>) {
            return "std::monostate";
        }
        // Otherwise use cxxabi
        return demangle(typeid(T));
    }

} // namespace constellation::utils
