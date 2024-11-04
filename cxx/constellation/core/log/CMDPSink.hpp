/**
 * @file
 * @brief Log sink for ZMQ communication
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>

#include <spdlog/async_logger.h>
#include <spdlog/sinks/base_sink.h>
#include <zmq.hpp>

#include "constellation/core/log/Level.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/utils/networking.hpp"

namespace constellation::log {
    /**
     * Sink log messages via CMDP
     *
     * Note that ZeroMQ sockets are not thread-safe, meaning that the sink requires a mutex.
     */
    class CMDPSink : public spdlog::sinks::base_sink<std::mutex> {
    public:
        /**
         * @brief Construct a new CMDPSink
         * @param context ZMQ context to be used
         */
        CMDPSink(std::shared_ptr<zmq::context_t> context);

        /**
         * @brief Deconstruct the CMDPSink
         */
        ~CMDPSink() override;

        // No copy/move constructor/assignment
        /// @cond doxygen_suppress
        CMDPSink(const CMDPSink& other) = delete;
        CMDPSink& operator=(const CMDPSink& other) = delete;
        CMDPSink(CMDPSink&& other) = delete;
        CMDPSink& operator=(CMDPSink&& other) = delete;
        /// @endcond

        /**
         * @brief Get ephemeral port this logger sink is bound to
         *
         * @return Port number
         */
        constexpr utils::Port getPort() const { return port_; }

        /**
         * @brief Set sender name and enable sending by starting the subscription thread
         *
         * @param sender_name Canonical name of the sender
         */
        void enableSending(std::string sender_name);

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) final;
        void flush_() final {}

    private:
        void subscription_loop(const std::stop_token& stop_token);

    private:
        std::unique_ptr<Logger> logger_;

        // Needs to store shared pointer since CMDPSink is owned by static SinkManager
        std::shared_ptr<zmq::context_t> context_;

        zmq::socket_t pub_socket_;
        utils::Port port_;
        std::string sender_name_;

        std::jthread subscription_thread_;
        std::map<std::string, std::map<Level, std::size_t>> log_subscriptions_;
    };

} // namespace constellation::log
