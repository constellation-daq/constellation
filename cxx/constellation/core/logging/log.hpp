/**
 * @file
 * @brief Log macros
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <atomic>
#include <string_view>

#include "constellation/core/logging/Level.hpp"

using enum constellation::log::Level;                // Forward log level enum
using namespace std::literals::string_view_literals; // NOLINT(google-global-names-in-headers)

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

/** Define a new token by concatenation */
#define LOG_CONCAT(x, y) x##y

/** Define new token by concatenation with support for nesting
 *
 * Nesting means that the parameters itself can be a macro. For example,`__LINE__` is a macro and `_VAR_NAME_L##__LINE__`
 * returns `_VAR_NAME_L__LINE__` instead of `_VAR_NAME_LXX`. See also https://stackoverflow.com/a/19666216.
 */
#define LOG_CONCAT_NESTED(x, y) LOG_CONCAT(x, y)

/** Defines a token for a unique log variable named `LOG_VAR_LXX` where `XX` is the line number */
#define LOG_VAR LOG_CONCAT_NESTED(LOG_VAR_L, __LINE__)

/**
 * Logs a message for a given level to a defined logger
 *
 * The stream expression is only evaluated if logging should take place.
 *
 * @param logger Logger on which to log
 * @param level Log level on which to log
 */
#define LOG_WITH_TOPIC(logger, level)                                                                                       \
    if((logger).shouldLog(level))                                                                                           \
    (logger).log(level)

/**
 * Logs a message for a given level to the default logger
 *
 * The stream expression is only evaluated if logging should take place.
 *
 * @param level Log level on which to log
 */
#define LOG_TO_DEFAULT(level) LOG_WITH_TOPIC(constellation::log::Logger::getDefault(), level)

/**
 * Helper macros which allow to chose the correct target macro (LOG_TO_DEFAULT or LOG_WITH_TOPIC) depending on the number
 * of arguments provided.
 *
 * * LOG_MACRO_CHOOSER will pass all its arguments along with the two possible target macros to LOG_FUNC_RECOMPOSER
 * * LOG_FUNC_RECOMPOSER passes all arguments including parentheses to the LOG_FUNC_CHOOSER
 * * LOG_FUNC_CHOOSER selects the third of its arguments as the function to be called:
 *                    * With one argument in __VA_ARGS__, this will be LOG_TO_DEFAULT
 *                    * With two arguments in __VA_ARGS__, this will be LOG_WITH_TOPIC
 * * Finally, the respective macro is called with its argument(s)
 */
#define LOG_FUNC_CHOOSER(_f1, _f2, _f3, ...) _f3
#define LOG_FUNC_RECOMPOSER(argsWithParentheses) LOG_FUNC_CHOOSER argsWithParentheses
#define LOG_MACRO_CHOOSER(...) LOG_FUNC_RECOMPOSER((__VA_ARGS__, LOG_WITH_TOPIC, LOG_TO_DEFAULT, ))

/**
 * Logging macro which takes either one or two arguments:
 *
 * LOG(level) will log to the default logger of the framework
 * LOG(logger, level) will log to the chosen logger instance
 */
#define LOG(...) LOG_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

/**
 * Logs a message if a given condition is true
 *
 * The given condition is evaluated after it was determined if logging should take place.
 *
 * @param logger Logger on which to log
 * @param level Log level on which to log
 * @param condition Condition to check before logging
 */
#define LOG_IF(logger, level, condition)                                                                                    \
    if((logger).shouldLog(level) && (condition))                                                                            \
    (logger).log(level)

/** Logs a message at most N times
 *
 * @param logger Logger on which to log
 * @param level Log level on which to log
 * @param count Count how often to log message at most
 */
#define LOG_N(logger, level, count)                                                                                         \
    static std::atomic_int LOG_VAR {count};                                                                                 \
    if(LOG_VAR > 0 && (logger).shouldLog(level))                                                                            \
    (logger).log(level) << (--LOG_VAR <= 0 ? "[further messages suppressed] "sv : ""sv)

/** Logs a message at most one time
 *
 * @param logger Logger on which to log
 * @param level Log level on which to log
 */
#define LOG_ONCE(logger, level) LOG_N(logger, level, 1)

// NOLINTEND(cppcoreguidelines-macro-usage)
