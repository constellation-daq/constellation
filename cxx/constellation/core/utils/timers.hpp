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
        StopwatchTimer() = default;
        void start() { start_time_ = std::chrono::steady_clock::now(); }
        void stop() { stop_time_ = std::chrono::steady_clock::now(); }
        std::chrono::nanoseconds duration() const {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(stop_time_ - start_time_);
        }

    private:
        std::chrono::steady_clock::time_point start_time_;
        std::chrono::steady_clock::time_point stop_time_;
    };

    /** Timer that can be used to wait for timeouts */
    class TimeoutTimer {
    public:
        TimeoutTimer() = default;
        TimeoutTimer(std::chrono::nanoseconds timeout) : timeout_(timeout) {}
        void reset() { start_time_ = std::chrono::steady_clock::now(); }
        void setTimeout(std::chrono::nanoseconds timeout) { timeout_ = timeout; }
        bool timeoutReached() const { return start_time_ + timeout_ < std::chrono::steady_clock::now(); }
        std::chrono::steady_clock::time_point startTime() const { return start_time_; }

    private:
        std::chrono::steady_clock::time_point start_time_;
        std::chrono::nanoseconds timeout_ {};
    };

} // namespace constellation::utils
