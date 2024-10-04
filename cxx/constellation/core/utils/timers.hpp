/**
 * @file
 * @brief Timer utilities
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>

namespace constellation::utils {
    /** Timer that can be used as a stopwatch */
    class StopwatchTimer {
    public:
        StopwatchTimer() { start(); }
        void start() { start_time_ = std::chrono::system_clock::now(); }
        void stop() { stop_time_ = std::chrono::system_clock::now(); }
        std::chrono::nanoseconds duration() const {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(stop_time_ - start_time_);
        }

    private:
        std::chrono::system_clock::time_point start_time_;
        std::chrono::system_clock::time_point stop_time_;
    };

    /** Timer that can be used to wait for timeouts */
    class TimeoutTimer {
    public:
        TimeoutTimer(std::chrono::nanoseconds timeout) : timeout_(timeout) { start(); }
        void start() { start_time_ = std::chrono::system_clock::now(); }
        bool timeoutReached() const { return start_time_ + timeout_ < std::chrono::system_clock::now(); }

    private:
        std::chrono::system_clock::time_point start_time_;
        std::chrono::nanoseconds timeout_;
    };

} // namespace constellation::utils
