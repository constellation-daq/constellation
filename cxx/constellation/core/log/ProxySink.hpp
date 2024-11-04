/**
 * @file
 * @brief ProxySink
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <memory>
#include <mutex>
#include <utility>

#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/sink.h>

namespace constellation::log {
    /**
     * Proxy sink for spdlog to define log level independent from global sink level
     */
    class ProxySink : public spdlog::sinks::base_sink<std::mutex> {
    public:
        /**
         * Construct a new proxy sink
         *
         * @param sink Shared pointer to sink for which to proxy
         */
        ProxySink(std::shared_ptr<spdlog::sinks::sink> sink) : sink_(std::move(sink)) {}

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) final {
            // Directly log message, ignore log level of underlying sink
            sink_->log(msg);
        }

        void flush_() final { sink_->flush(); }

    private:
        std::shared_ptr<spdlog::sinks::sink> sink_;
    };

} // namespace constellation::log
