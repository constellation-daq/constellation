/**
 * @file
 * @brief Data sender for satellites
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <string>

#include <zmq.hpp>

#include "constellation/build.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/config/Value.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/utils/ports.hpp"

namespace constellation::data {
    class DataSender {
    public:
        /** CDTP message wrapper */
        class DataMessage : private message::CDTP1Message {
        public:
            /**
             * @brief Add data in a new frame to the message
             *
             * @param data Data to add
             */
            void addDataFrame(message::PayloadBuffer&& data) { addPayload(std::move(data)); }

            /**
             * @brief Add a tag to the header of the message
             *
             * @param key Key of the tag
             * @param value Value of the tag
             */
            void addTag(const std::string& key, config::Value value) { getHeader().setTag(key, std::move(value)); }

        private:
            // DataSender needs access to constructor
            friend DataSender;

            DataMessage(std::string sender, std::uint64_t seq, std::size_t frames)
                : message::CDTP1Message({std::move(sender), seq, message::CDTP1Message::Type::DATA}, frames) {}
        };

    private:
        enum class State : std::uint8_t {
            BEFORE_BOR,
            IN_RUN,
        };

    public:
        /**
         * @brief Construct new data sender
         *
         * @param sender_name Canonical name of the sender
         */
        CNSTLN_API DataSender(std::string sender_name);

        /**
         * @brief Initialize data sender
         *
         * Reads the following config parameters:
         * * `_data_bor_timeout`
         * * `_data_eor_timeout`
         *
         * @param config Configuration from initializing transition
         */
        CNSTLN_API void initializing(config::Configuration& config);

        /**
         * @brief Reconfigure data sender
         *
         * Supports reconfiguring of the following config paramets:
         * * `_data_bor_timeout`
         * * `_data_eor_timeout`
         */
        CNSTLN_API void reconfiguring(const config::Configuration& partial_config);

        /**
         * @brief Start data sender by sending begin of run
         *
         * @throw SendTimeoutError If the `_data_bor_timeout` is reached
         *
         * @param config Configuration to send in BOR message
         */
        CNSTLN_API void starting(const config::Configuration& config);

        /**
         * @brief Create new message for attaching data frames
         *
         * @note This function increases the sequence number
         * @note To send the data message, use `sendDataMessage()`
         *
         * @param frames Number of data frames to reserve
         */
        CNSTLN_API DataMessage newDataMessage(std::size_t frames = 1);

        /**
         * @brief Send data message created with `newDataMessage()`
         *
         * @note The return value of this function *has* to be checked. If it is `false`, one should take action such as
         *       discarding the message, trying to send it again or throwing an exception.
         *
         * @param message Reference to data message
         * @return If the message was successfully sent/queued
         */
        CNSTLN_API [[nodiscard]] bool sendDataMessage(DataMessage& message);

        /**
         * @brief Set the run metadata send at the end of the run
         *
         * @param run_metadata Optional run metadata
         */
        CNSTLN_API void setRunMetadata(config::Dictionary run_metadata);

        /**
         * @brief Stop data sender by sending end of run
         *
         * @throw SendTimeoutError If the `_data_eor_timeout` is reached
         */
        CNSTLN_API void stopping();

        /**
         * @return Ephemeral port to which the data sender socket is bound
         */
        constexpr utils::Port getPort() const { return port_; }

    private:
        /**
         * @brief Set send timeout
         *
         * @param timeout Timeout, -1 is infinite (block until sent)
         */
        void set_send_timeout(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1));

    private:
        zmq::context_t context_;
        zmq::socket_t socket_;
        utils::Port port_;
        std::string sender_name_;
        log::Logger logger_;
        State state_;
        std::chrono::seconds data_bor_timeout_ {};
        std::chrono::seconds data_eor_timeout_ {};
        std::size_t seq_ {};
        config::Dictionary run_metadata_;
    };
} // namespace constellation::data
