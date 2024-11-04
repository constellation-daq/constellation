/**
 * @file
 * @brief Logger
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <memory>
#include <source_location>
#include <sstream>
#include <string_view>
#include <utility>

#include <spdlog/async_logger.h>

#include "constellation/build.hpp"
#include "constellation/core/log/Level.hpp"

namespace constellation::log {
    /**
     * Logger class that to log messages via CMDP and to the console
     *
     * This class implements a wrapper around the spdlog logger and provides additional features such as the possibility to
     * perform logging using streams (with << syntax rather than enclosing the log message in parentheses)
     */
    class Logger {
    public:
        /**
         * Log stream that executes logging upon its destruction
         */
        class LogStream final : public std::ostringstream {
        public:
            LogStream(const Logger& logger, Level level, std::source_location src_loc)
                : logger_(logger), level_(level), src_loc_(src_loc) {}
            ~LogStream() final { logger_.log(level_, this->view(), src_loc_); } // NOLINT(bugprone-exception-escape)

            // No copy/move constructor/assignment
            /// @cond doxygen_suppress
            LogStream(const LogStream& other) = delete;
            LogStream& operator=(const LogStream& other) = delete;
            LogStream(LogStream&& other) = delete;
            LogStream& operator=(LogStream&& other) = delete;
            /// @endcond

        private:
            const Logger& logger_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
            Level level_;
            std::source_location src_loc_;
        };

    public:
        /**
         * @brief Constructor a new logger
         *
         * @param topic Topic of the logger (which is the name of the spdlog logger)
         */
        CNSTLN_API Logger(std::string_view topic);

        /**
         * @brief Return the default logger
         */
        CNSTLN_API static Logger& getDefault();

        CNSTLN_API virtual ~Logger();

        // No copy/move constructor/assignment
        /// @cond doxygen_suppress
        Logger(const Logger& other) = delete;
        Logger& operator=(const Logger& other) = delete;
        Logger(Logger&& other) = delete;
        Logger& operator=(Logger&& other) = delete;
        /// @endcond

        /**
         * @brief Check if a message should be logged given the currently configured log level
         *
         * @param level Log level to be tested against the logger configuration
         * @return Boolean indicating if the message should be logged
         */
        CNSTLN_API bool shouldLog(Level level) const { return spdlog_logger_->should_log(to_spdlog_level(level)); }

        /**
         * @brief Return the current log level of the logger
         *
         * @note This should not be used to determine if logging should take place, instead `shouldLog()` should be used
         *
         * @return Current log level
         */
        CNSTLN_API Level getLogLevel() const { return from_spdlog_level(spdlog_logger_->level()); }

        /**
         * @brief Log a message using a stream
         *
         * @param level Log level of the log message
         * @param src_loc Source code location from which the log message emitted
         * @return `LogStream` to which the log message can be streamed
         */
        CNSTLN_API LogStream log(Level level, std::source_location src_loc = std::source_location::current()) const {
            return {*this, level, src_loc};
        }

        /**
         * @brief Log a message
         *
         * @param level Level of the log message
         * @param message Log message
         * @param src_loc Source code location from which the log message emitted
         */
        CNSTLN_API void log(Level level,
                            std::string_view message,
                            std::source_location src_loc = std::source_location::current()) const {
            spdlog_logger_->log({src_loc.file_name(), static_cast<int>(src_loc.line()), src_loc.function_name()},
                                to_spdlog_level(level),
                                message);
        }

        /**
         * @brief Flush each spdlog sink (synchronously)
         */
        CNSTLN_API void flush();

    protected:
        Logger(std::shared_ptr<spdlog::async_logger> spdlog_logger) : spdlog_logger_(std::move(spdlog_logger)) {}

    private:
        std::shared_ptr<spdlog::async_logger> spdlog_logger_;
    };
} // namespace constellation::log
