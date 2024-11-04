/**
 * @file
 * @brief Log macros
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <atomic> // IWYU pragma: keep

#include "constellation/core/log/Level.hpp"  // IWYU pragma: export
#include "constellation/core/log/Logger.hpp" // IWYU pragma: keep

using enum constellation::log::Level; // Forward log level enum

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

/// @cond doxygen_suppress

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
#define LOG_2ARGS(logger, level)                                                                                            \
    if((logger).shouldLog(level))                                                                                           \
    (logger).log(level)

/**
 * Logs a message for a given level to the default logger
 *
 * The stream expression is only evaluated if logging should take place.
 *
 * @param level Log level on which to log
 */
#define LOG_1ARG(level) LOG_2ARGS(constellation::log::Logger::getDefault(), level)

/**
 * Logs a message if a given condition is true
 *
 * The given condition is evaluated after it was determined if logging should take place.
 *
 * @param logger Logger on which to log
 * @param level Log level on which to log
 * @param condition Condition to check before logging
 */
#define LOG_IF_3ARGS(logger, level, condition)                                                                              \
    if((logger).shouldLog(level) && (condition))                                                                            \
    (logger).log(level)

/**
 * Logs a message to the default logger if a given condition is true
 *
 * The given condition is evaluated after it was determined if logging should take place.
 *
 * @param level Log level on which to log
 * @param condition Condition to check before logging
 */
#define LOG_IF_2ARGS(level, condition) LOG_IF_3ARGS(constellation::log::Logger::getDefault(), level, condition)

/** Logs a message at most N times
 *
 * @param logger Logger on which to log
 * @param level Log level on which to log
 * @param count Count how often to log message at most
 */
#define LOG_N_3ARGS(logger, level, count)                                                                                   \
    static thread_local std::atomic_int LOG_VAR {count};                                                                    \
    if(LOG_VAR > 0 && (logger).shouldLog(level))                                                                            \
    (logger).log(level) << (--LOG_VAR <= 0 ? "[further messages suppressed] " : "")

/** Logs a message at most N times to the default logger
 *
 * @param level Log level on which to log
 * @param count Count how often to log message at most
 */
#define LOG_N_2ARGS(level, count) LOG_N_3ARGS(constellation::log::Logger::getDefault(), level, count)

/** Logs a message at most one time
 *
 * @param logger Logger on which to log
 * @param level Log level on which to log
 */
#define LOG_ONCE_2ARGS(logger, level) LOG_N_3ARGS(logger, level, 1)

/** Logs a message at most one time to the default logger
 *
 * @param level Log level on which to log
 */
#define LOG_ONCE_1ARG(level) LOG_ONCE_2ARGS(constellation::log::Logger::getDefault(), level)

/** Logs a message every N calls
 *
 * @param logger Logger on which to log
 * @param level Log level on which to log
 * @param count Interval at which the message should be logged
 */
#define LOG_NTH_3ARGS(logger, level, count)                                                                                 \
    static thread_local std::atomic_size_t LOG_VAR {0};                                                                     \
    if(LOG_VAR++ % (count) == 0 && (logger).shouldLog(level))                                                               \
    (logger).log(level)

/** Logs a message every N calls to the default logger
 *
 * @param level Log level on which to log
 * @param count Interval at which the message should be logged
 */
#define LOG_NTH_2ARGS(level, count) LOG_NTH_3ARGS(constellation::log::Logger::getDefault(), level, count)

/**
 * Helper macros which allow to chose the correct target macro (_1ARG, _2ARGS or _3ARGS) depending on the number of arguments
 * and the macro prefix (F##) provided.
 *
 * * LOG_MACRO passes the macro prefix and all arguments to the LOG_MACRO_CHOOSER
 * * LOG_MACRO_CHOOSER will pass all its arguments along with the three possible target macros to LOG_FUNC_RECOMPOSER. It
 *                    also resolves the actual macro name from the macro prefix provided by LOG_MACRO.
 * * LOG_FUNC_RECOMPOSER passes all arguments including parentheses to the LOG_FUNC_CHOOSER
 * * LOG_FUNC_CHOOSER selects the third of its arguments as the function to be called:
 *                    * With one argument in __VA_ARGS__, this will be LOG_TO_DEFAULT
 *                    * With two arguments in __VA_ARGS__, this will be LOG_WITH_TOPIC
 * * Finally, the respective macro is called with its argument(s)
 */
#define LOG_FUNC_CHOOSER(_f1, _f2, _f3, _f4, ...) _f4
#define LOG_FUNC_RECOMPOSER(argsWithParentheses) LOG_FUNC_CHOOSER argsWithParentheses
#define LOG_MACRO_CHOOSER(F, ...) LOG_FUNC_RECOMPOSER((__VA_ARGS__, F##_3ARGS, F##_2ARGS, F##_1ARG, ))
#define LOG_MACRO(FUNC, ...) LOG_MACRO_CHOOSER(FUNC, __VA_ARGS__)(__VA_ARGS__)

/// @endcond

/**
 * Logs a message for a given level either to the default logger or a defined logger. The stream expression is only
 * evaluated if logging should take place.
 *
 * This macro takes either one or two arguments:
 *
 * * `LOG(level)` will log to the default logger of the framework
 * * `LOG(logger, level)` will log to the chosen logger instance
 *
 * `logger` is the optional \ref constellation::log::Logger instance to use and `level` indicates the verbosity level on
 * which to log.
 */
#define LOG(...) LOG_MACRO(LOG, __VA_ARGS__)

/**
 * Logs a message if a given condition is true to either the default logger or a defined logger. The given condition is
 * evaluated after it was determined if logging should take place based on the provided level.
 *
 * This macro takes either two or three arguments:
 *
 * * `LOG_IF(level, condition)` will log to the default logger of the framework
 * * `LOG_IF(logger, level, condition)` will log to the chosen logger instance
 *
 * `logger` is the optional \ref constellation::log::Logger instance to use, `level` indicates the verbosity level on which
 * to log and `condition` represents the condition to be fulfilled before logging.
 */
#define LOG_IF(...) LOG_MACRO(LOG_IF, __VA_ARGS__)

/**
 * Logs a message at most N times either to the default logger or a defined logger.
 *
 * This macro takes either two or three arguments:
 *
 * * `LOG_N(level, count)` will log to the default logger of the framework
 * * `LOG_N(logger, level, count)` will log to the chosen logger instance
 *
 * `logger` is the optional \ref constellation::log::Logger instance to use, `level` indicates the verbosity level on which
 * to log and `count` is the maximum number of times this message is logged.
 */
#define LOG_N(...) LOG_MACRO(LOG_N, __VA_ARGS__)

/**
 * Logs a message at most one time either to the default logger or a defined logger. This macro is equivalent to LOG_N(...,
 * 1).
 *
 * This macro takes either one or two arguments:
 *
 * * `LOG_ONCE(level)` will log to the default logger of the framework
 * * `LOG_ONCE(logger, level)` will log to the chosen logger instance
 *
 * `logger` is the optional \ref constellation::log::Logger instance to use and `level` indicates the verbosity level on
 * which to log.
 */
#define LOG_ONCE(...) LOG_MACRO(LOG_ONCE, __VA_ARGS__)

/**
 * Logs a message every Nth time the logging macro is called either to the default logger or a defined logger.
 *
 * This macro takes either two or three arguments:
 *
 * * `LOG_NTH(level, count)` will log to the default logger of the framework
 * * `LOG_NTH(logger, level, count)` will log to the chosen logger instance
 *
 * `logger` is the optional \ref constellation::log::Logger instance to use, `level` indicates the verbosity level on which
 * to log and `count` is the interval of calls to the macro at which the message will be logged.
 */
#define LOG_NTH(...) LOG_MACRO(LOG_NTH, __VA_ARGS__)

// NOLINTEND(cppcoreguidelines-macro-usage)
