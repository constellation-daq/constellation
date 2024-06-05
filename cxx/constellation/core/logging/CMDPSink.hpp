/**
 * @file
 * @brief Log sink for ZMQ communication
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <map>
#include <mutex>
#include <string>
#include <thread>

#include <spdlog/sinks/base_sink.h>
#include <zmq.hpp>

#include "constellation/core/logging/Level.hpp"
#include "constellation/core/utils/ports.hpp"

namespace constellation::log {
    /**
     * Sink log messages via CMDP
     *
     * Note that ZeroMQ sockets are not thread-safe, meaning that the sink requires a mutex.
     */
    class CMDPSink : public spdlog::sinks::base_sink<std::mutex> {
    public:
        /**
         * Construct a new CMDPSink
         */
        CMDPSink();

        /**
         * Deconstruct the CMDPSink
         */
        ~CMDPSink() override;

        // No copy/move constructor/assignment
        CMDPSink(const CMDPSink& other) = delete;
        CMDPSink& operator=(const CMDPSink& other) = delete;
        CMDPSink(CMDPSink&& other) = delete;
        CMDPSink& operator=(CMDPSink&& other) = delete;

        /**
         * Get ephemeral port this logger sink is bound to
         *
         * @return Port number
         */
        constexpr utils::Port getPort() const { return port_; }

        /**
         * Set sender name
         *
         * @param sender_name Canonical name of the sender
         */
        void setSender(std::string sender_name);

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) final;
        void flush_() final {}

    private:
        zmq::context_t context_;
        zmq::socket_t publisher_;

        std::jthread subscription_thread_;
        void subscription_loop(const std::stop_token& stop_token);
        std::map<std::string, std::map<Level, std::size_t>> log_subscriptions_;

        utils::Port port_;
        std::string sender_name_;
        std::once_flag setup_flag_;
    };

} // namespace constellation::log
