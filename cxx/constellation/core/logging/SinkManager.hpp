/**
 * @file
 * @brief Log Sink Manager
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <ctime>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/async_logger.h>
#include <spdlog/common.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "constellation/build.hpp"
#include "constellation/core/logging/CMDPSink.hpp"
#include "constellation/core/logging/Level.hpp"
#include "constellation/core/utils/ports.hpp"

namespace constellation::log {
    /**
     * Global sink manager
     *
     * This class manager the console and CMDP sinks and can creates new spdlog loggers.
     */
    class SinkManager {
    private:
        // Formatter for the log level (overwrites spdlog defaults)
        class CNSTLN_API ConstellationLevelFormatter final : public spdlog::custom_flag_formatter {
        public:
            ConstellationLevelFormatter(bool format_short);
            void format(const spdlog::details::log_msg& msg, const std::tm& tm, spdlog::memory_buf_t& dest) override;
            std::unique_ptr<spdlog::custom_flag_formatter> clone() const override;

        private:
            bool format_short_;
        };

        // Formatter for the topic (adds brackets except for the default logger)
        class CNSTLN_API ConstellationTopicFormatter final : public spdlog::custom_flag_formatter {
        public:
            void format(const spdlog::details::log_msg& msg, const std::tm& tm, spdlog::memory_buf_t& dest) override;
            std::unique_ptr<spdlog::custom_flag_formatter> clone() const override;
        };

    public:
        CNSTLN_API static SinkManager& getInstance();

        ~SinkManager() = default;

        // No copy/move constructor/assignment
        SinkManager(const SinkManager& other) = delete;
        SinkManager& operator=(const SinkManager& other) = delete;
        SinkManager(SinkManager&& other) = delete;
        SinkManager& operator=(SinkManager&& other) = delete;

        /**
         * Set the global (default) console log level
         *
         * @param level Log level for console output
         */
        CNSTLN_API void setGlobalConsoleLevel(Level level);

        /**
         * Get the ephemeral port to which the CMDP sink is bound to
         *
         * @return Port number
         */
        utils::Port getCMDPPort() const { return cmdp_sink_->getPort(); }

        /**
         * Enable sending via CMDP
         *
         * @note In most cases if this is used, the first a program should do is call `enableCMDPBacktrace`.
         *
         * @param sender_name Canonical name of the satellite
         */
        CNSTLN_API void enableCMDPSending(std::string sender_name);

        /**
         * Create a new asynchronous spdlog logger
         *
         * @param topic Topic of the new logger
         * @param console_level Optional log level for console output to overwrite global level
         * @return Shared pointer to the new logger
         */
        CNSTLN_API std::shared_ptr<spdlog::async_logger> createLogger(std::string topic,
                                                                      std::optional<Level> console_level = std::nullopt);

        /**
         * Return the default logger (no topic)
         */
        std::shared_ptr<spdlog::async_logger> getDefaultLogger() const { return default_logger_; }

        /**
         * Update individual logger levels from CMDP subscriptions
         *
         * @param cmdp_global_level Global subscription level
         * @param cmdp_sub_topic_levels Map of individual logger subscription levels
         */
        CNSTLN_API void updateCMDPLevels(Level cmdp_global_level,
                                         std::map<std::string_view, Level> cmdp_sub_topic_levels = {});

    private:
        SinkManager();

        /**
         * Set the CMDP log level for a particular logger given the current subscriptions
         *
         * @param logger Logger for which to set the log level
         */
        void set_cmdp_level(std::shared_ptr<spdlog::async_logger>& logger);

    private:
        std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink_;
        std::shared_ptr<CMDPSink> cmdp_sink_;

        std::shared_ptr<spdlog::async_logger> default_logger_;
        std::shared_ptr<spdlog::async_logger> cmdp_console_logger_;

        std::vector<std::shared_ptr<spdlog::async_logger>> loggers_;

        Level cmdp_global_level_;
        std::map<std::string_view, Level> cmdp_sub_topic_levels_;
    };
} // namespace constellation::log
