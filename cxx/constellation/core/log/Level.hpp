/**
 * @file
 * @brief Log levels
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <spdlog/common.h>

#include "constellation/core/utils/std_future.hpp"

namespace constellation::log {
    /** Log levels for the Constellation framework
     *
     * The chosen values allows for direct casting to spdlog::level::level_enum and correspond to the levels defined in the
     * CMDP protocol.
     */
    enum class Level : int { // NOLINT(performance-enum-size)
        /** verbose information which allows to follow the call stack of the host program. */
        TRACE = 0,

        /** information relevant to developers for debugging the host program. */
        DEBUG = 1,

        /** information on regular events intended for end users of the host program. */
        INFO = 2,

        /** notify the end user of the host program of unexpected events which require further investigation. */
        WARNING = 3,

        /** communicate important information about the host program to the end user with low frequency. */
        STATUS = 4,

        /** notify the end user about critical events which require immediate attention and MAY have triggered an automated
           response by the host program or other hosts. */
        CRITICAL = 5,

        /** no logging */
        OFF = 6,
    };
    using enum Level;

    /**
     * Helper function to convert Constellation verbosity levels to spdlog::Level::level_enum values
     *
     * @param level Constellation verbosity level
     * @return spdlog level value
     */
    constexpr spdlog::level::level_enum to_spdlog_level(Level level) {
        return static_cast<spdlog::level::level_enum>(level);
    }

    /**
     * Helper function to convert spdlog::Level::level_enum values to Constellation verbosity levels
     *
     * @param level spdlog level value
     * @return Constellation verbosity constellation::log::Level
     */
    constexpr Level from_spdlog_level(spdlog::level::level_enum level) {
        return static_cast<Level>(level);
    }

    /**
     * Compare two logging levels and return the lower one
     *
     * @tparam LevelL left logging level type
     * @tparam LevelR right logging level type
     * @param lhs Left logging level
     * @param rhs Right logging level
     * @return Minimum Constellation verbosity constellation::log::Level
     */
    template <typename LevelL, typename LevelR> constexpr Level min_level(LevelL lhs, LevelR rhs) {
        return static_cast<Level>(std::min(std::to_underlying(lhs), std::to_underlying(rhs)));
    }
} // namespace constellation::log
