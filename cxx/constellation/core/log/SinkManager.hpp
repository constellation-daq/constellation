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
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/async_logger.h>
#include <spdlog/common.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <zmq.hpp>

#include "constellation/build.hpp"
#include "constellation/core/log/CMDPSink.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/utils/networking.hpp"

namespace constellation::log {
    /**
     * @brief Global sink manager
     *
     * This class manager the console and CMDP sinks and can creates new spdlog loggers.
     */
    class SinkManager {
    private:
        // Formatter for the log level (overwrites spdlog defaults)
        class ConstellationLevelFormatter : public spdlog::custom_flag_formatter {
        public:
            ConstellationLevelFormatter(bool format_short);
            void format(const spdlog::details::log_msg& msg, const std::tm& tm, spdlog::memory_buf_t& dest) override;
            std::unique_ptr<spdlog::custom_flag_formatter> clone() const override;

        private:
            bool format_short_;
        };

        // Formatter for the topic (adds brackets except for the default logger)
        class ConstellationTopicFormatter : public spdlog::custom_flag_formatter {
        public:
            void format(const spdlog::details::log_msg& msg, const std::tm& tm, spdlog::memory_buf_t& dest) override;
            std::unique_ptr<spdlog::custom_flag_formatter> clone() const override;
        };

    public:
        CNSTLN_API static SinkManager& getInstance();

        ~SinkManager() = default;

        // No copy/move constructor/assignment
        /// @cond doxygen_suppress
        SinkManager(const SinkManager& other) = delete;
        SinkManager& operator=(const SinkManager& other) = delete;
        SinkManager(SinkManager&& other) = delete;
        SinkManager& operator=(SinkManager&& other) = delete;
        /// @endcond

        /**
         * @brief Get the ephemeral port to which the CMDP sink is bound to
         *
         * @return Port number
         */
        utils::Port getCMDPPort() const { return cmdp_sink_->getPort(); }

        /**
         * @brief Enable sending via CMDP
         *
         * @param sender_name Canonical name of the satellite
         */
        CNSTLN_API void enableCMDPSending(std::string sender_name);

        /**
         * @brief Get an asynchronous spdlog logger with a given topic
         *
         * This creates a new logger if no logger with the given topic exists
         *
         * @param topic Topic of the logger
         * @return Shared pointer to the logger
         */
        CNSTLN_API std::shared_ptr<spdlog::async_logger> getLogger(std::string_view topic);

        /**
         * @brief Return the default logger
         */
        std::shared_ptr<spdlog::async_logger> getDefaultLogger() const { return default_logger_; }

        /**
         * @brief Set the console log levels
         *
         * @param global_level Global log level for console output
         * @param topic_levels Log level overwrites for specific topics
         */
        CNSTLN_API void setConsoleLevels(Level global_level, std::map<std::string, Level> topic_levels = {});

        /**
         * @brief Update individual logger levels from CMDP subscriptions
         *
         * @param cmdp_global_level Global subscription level
         * @param cmdp_sub_topic_levels Map of individual logger subscription levels
         */
        CNSTLN_API void updateCMDPLevels(Level cmdp_global_level,
                                         std::map<std::string_view, Level> cmdp_sub_topic_levels = {});

    private:
        SinkManager();

        /**
         * @brief Create a new asynchronous spdlog logger
         *
         * @param topic Topic of the logger
         * @return Shared pointer to the new logger
         */
        std::shared_ptr<spdlog::async_logger> create_logger(std::string_view topic);

        /**
         * @brief Calculate the CMDP and console log level for a particular logger given the current subscriptions
         *
         * @param logger Logger for which to set the log level
         */
        void calculate_log_level(std::shared_ptr<spdlog::async_logger>& logger);

    private:
        std::shared_ptr<zmq::context_t> zmq_context_;

        std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink_;
        std::shared_ptr<CMDPSink> cmdp_sink_;

        std::shared_ptr<spdlog::async_logger> default_logger_;

        std::vector<std::shared_ptr<spdlog::async_logger>> loggers_;
        std::mutex loggers_mutex_;

        Level console_global_level_;
        std::map<std::string, Level> console_topic_levels_;
        Level cmdp_global_level_;
        std::map<std::string_view, Level> cmdp_sub_topic_levels_;
        std::mutex levels_mutex_;
    };
} // namespace constellation::log
