/**
 * @file
 * @brief Log levels
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "constellation/core/utils/std_future.hpp"

#include <spdlog/common.h>

namespace constellation::log {
    /** Log levels for framework
     *
     * The chosen values allows for direct casting to spdlog::level::level_enum
     */
    enum class Level : int {
        /** */
        TRACE = 0,

        /** */
        DEBUG = 1,

        /** */
        INFO = 2,

        /** */
        WARNING = 3,

        /** */
        STATUS = 4,

        /** */
        CRITICAL = 5,

        OFF = 6,
    };
    using enum Level;

    constexpr spdlog::level::level_enum to_spdlog_level(Level level) {
        return static_cast<spdlog::level::level_enum>(level);
    }

    constexpr Level from_spdlog_level(spdlog::level::level_enum level) {
        return static_cast<Level>(level);
    }

    template <typename LevelL, typename LevelR> constexpr Level min_level(LevelL lhs, LevelR rhs) {
        return static_cast<Level>(std::min(std::to_underlying(lhs), std::to_underlying(rhs)));
    }
} // namespace constellation::log
