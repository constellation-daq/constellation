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
#include "constellation/core/logging/Logger.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/utils/ports.hpp"

namespace constellation::data {
    class DataSender {
    private:
        enum class State : std::uint8_t {
            BEFORE_BOR,
            IN_RUN,
            IN_MESSAGE,
        };

    public:
        CNSTLN_API DataSender(std::string sender_name);

        /**
         * @brief Initialize data sender
         *
         * Reads the `_data_bor_timeout` and `_data_eor_timeout` config settings.
         *
         * @param config Configuration from initializing transition
         */
        CNSTLN_API void initializing(config::Configuration& config);

        /**
         * @brief Start data sender by sending begin of run
         *
         * @note This function throws if the `_data_bor_timeout` is reached
         *
         * @param config Configuration to send in BOR message
         */
        CNSTLN_API void starting(const config::Configuration& config);

        /**
         * @brief Begin a new message for attaching data frames
         *
         * @note To send the data message, use `sendDataMessage()`.
         *
         * @param frames Number of data frames to reserve
         */
        CNSTLN_API void newDataMessage(std::size_t frames = 1);

        /**
         * @brief Add a data frame to the next message
         *
         * @note Requires an open data message created by `newDataMessage()`
         *
         * @param data Payload for the frame
         */
        CNSTLN_API void addDataToMessage(message::PayloadBuffer data);

        /**
         * @brief Send all data frames in message
         *
         * @note The return value of this function *has* to be checked. If it is `false`, one should take action such as
         *       discarding the message, trying to send it again or throwing an exception.
         *
         * @return If the message was successfully sent/queued
         */
        CNSTLN_API [[nodiscard]] bool sendDataMessage();

        /**
         * @brief Send a data message with a single data frame directly
         *
         * Equivalent to
         * ```
         * newDataMessage();
         * addDataToMessage(data);
         * sendDataMessage();
         * ```
         *
         * @note See `sendDataMessage()` for details on the return value
         *
         * @param data Payload for the (first data frame of the) message
         * @return If the message was successfully sent/queued
         */
        CNSTLN_API [[nodiscard]] bool sendData(message::PayloadBuffer data);

        /**
         * @brief Set the run metadata send at the end of the run
         *
         * @param run_metadata Optional run metadata
         */
        CNSTLN_API void setRunMetadata(config::Dictionary run_metadata);

        /**
         * @brief Stop data sender by sending end of run
         *s
         * @note This function throws if the `_data_eor_timeout` is reached
         */
        CNSTLN_API void stopping();

    private:
        /** Set send timeout, -1 is infinite (block until sent) */
        void set_send_timeout(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1));

        /** Send the stored message with dontwait flag, return if able to queue */
        [[nodiscard]] bool send_message();

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
        message::CDTP1Message msg_;
        config::Dictionary run_metadata_;
    };
} // namespace constellation::data
