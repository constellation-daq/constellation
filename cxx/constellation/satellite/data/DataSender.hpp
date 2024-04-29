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

        /** Initialize data sender */
        CNSTLN_API void initializing(config::Configuration& config);

        /** Starting: send begin of run */
        CNSTLN_API void starting(const config::Configuration& config);

        /** Begin a new message for attaching data frames */
        CNSTLN_API void newDataMessage(std::size_t frames = 1);

        /** Add a data frame to the next message */
        CNSTLN_API void addDataToMessage(message::PayloadBuffer data);

        /** Send all data frames in message */
        CNSTLN_API void sendDataMessage();

        /** Send a data message with a single data frame directly */
        CNSTLN_API void sendData(message::PayloadBuffer data);

        /** Set the run metadata send at the end of the run */
        CNSTLN_API void setRunMetadata(config::Dictionary run_metadata);

        /** Stopping: send end of run */
        CNSTLN_API void stopping();

    private:
        /** Set send timeout, -1 is infinite (block until sent) */
        void set_send_timeout(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1));

        /** Send the stored message, return if able to queue */
        bool send_message();

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
