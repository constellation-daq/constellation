/**
 * @file
 * @brief Utilities for std::chrono objects
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <string>
#include <version>

namespace constellation::utils {

    template <typename T>
    concept chrono_time_point = requires(T t) { std::chrono::time_point(t); };

    template <typename T>
    concept chrono_duration = requires(T t) { std::chrono::duration(t); };

} // namespace constellation::utils

#ifdef __cpp_lib_format
#include <format>

namespace constellation::utils {

    template <typename T>
        requires chrono_time_point<T>
    inline std::string time_point_to_string(T tp) {
        return std::format("{0:%F} {0:%T}", tp);
    }

    template <typename T>
        requires chrono_duration<T>
    inline std::string duration_to_string(T d) {
        return std::format("{}", d);
    }

} // namespace constellation::utils

#else
#include <ctime>
#include <iomanip>
#include <sstream>
#include <time.h>

namespace constellation::utils {

    template <typename T>
        requires chrono_time_point<T>
    inline std::string time_point_to_string(T tp) {
        // Convert to system_clock to get time_t
        const auto tp_sys = tp; // std::chrono::clock_cast<std::chrono::system_clock>(tp);
        // Convert time point to tm struct
        const auto time_t = std::chrono::system_clock::to_time_t(tp_sys);
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
    }

} // namespace constellation::utils

#endif
