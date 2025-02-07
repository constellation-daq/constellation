/**
 * @file
 * @brief Data transmitting satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <zmq.hpp>

#include "constellation/build.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/networking/Port.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/BaseSatellite.hpp"
#include "constellation/satellite/Satellite.hpp"

namespace constellation::satellite {
    /**
     * @brief Satellite class with additional functions to transmit data
     */
    class CNSTLN_API TransmitterSatellite : public Satellite {
    private:
        /**
         * @brief Wrapper for a CDTP message
         */
        class DataMessage : private message::CDTP1Message {
        public:
            /**
             * @brief Add data in a new frame to the message
             *
             * @param data Data to add (see `PayloadBuffer` class for details)
             */
            void addFrame(message::PayloadBuffer&& data) { addPayload(std::move(data)); }

            /**
             * @brief Add a tag to the header of the message
             *
             * @param key Key of the tag
             * @param value Value of the tag
             */
            template <typename T> void addTag(const std::string& key, const T& value) { getHeader().setTag(key, value); }

            /**
             * @brief Obtain current number of frames in this message
             *
             * @return Current number of data frames
             */
            std::size_t countFrames() const { return countPayloadFrames(); }

            /** Allow using the header to get e.g. sequence number */
            using message::CDTP1Message::getHeader;

        private:
            // TransmitterSatellite needs access to constructor
            friend TransmitterSatellite;

            DataMessage(std::string sender, std::uint64_t seq, std::size_t frames)
                : message::CDTP1Message({std::move(sender), seq, message::CDTP1Message::Type::DATA}, frames) {}
        };

    public:
        /**
         * @brief Create new message for attaching data frames
         *
         * @note This function increases the CDTP sequence number.
         * @note To send the data message, use `sendDataMessage()`.
         *
         * @param frames Number of data frames to reserve
         */
        DataMessage newDataMessage(std::size_t frames = 1);

        /**
         * @brief Attempt to send data message created with `newDataMessage()`
         *
         * @note The return value of this function *has* to be checked. If it is `false`, one should take action such as
         *       discarding the message, trying to send it again or throwing an exception.
         *
         * @param message Reference to data message
         * @return True if the message was successfully sent/queued, false otherwise
         */
        [[nodiscard]] bool trySendDataMessage(DataMessage& message);

        /**
         * @brief Send data message created with `newDataMessage()`
         *
         * @note This method will block until the message has been sent *or* the timeout for sending data messages has been
         *       reached. In the latter case, a SendTimeoutError exception is thrown.
         *
         * @param message Reference to data message
         * @throw SendTimeoutError If data send timeout is reached
         */
        void sendDataMessage(DataMessage& message);

        /**
         * @brief Mark this run data as tainted
         * @details This will set the condition tag in the run metadata to `TAINTED` instead of `GOOD` to mark that there
         *          might be an issue with the data recorded during this run.
         */
        void markRunTainted() { mark_run_tainted_ = true; };

        /**
         * @brief Set tag for the BOR message metadata send at the begin of the run
         */
        template <typename T> void setBORTag(std::string_view key, const T& value) {
            bor_tags_[utils::transform(key, ::tolower)] = value;
        }

        /**
         * @brief Set tag for the EOR message metadata send at the end of the run
         */
        template <typename T> void setEORTag(std::string_view key, const T& value) {
            eor_tags_[utils::transform(key, ::tolower)] = value;
        }

        /**
         * @brief Return the ephemeral port number to which the CDTP socket is bound to
         */
        constexpr networking::Port getDataPort() const { return cdtp_port_; }

    protected:
        /**
         * @brief Construct a data transmitting satellite
         *
         * @param type Satellite type
         * @param name Name of this satellite instance
         */
        TransmitterSatellite(std::string_view type, std::string_view name);

    private:
        // Needs access to transmitter specific functions
        friend BaseSatellite;

        /**
         * @brief Initialize transmitter components of satellite
         *
         * Reads the following config parameters:
         * * `_bor_timeout`
         * * `_eor_timeout
         * * `_data_timeout`
         *
         * @param config Configuration of the satellite
         */
        void initializing_transmitter(config::Configuration& config);

        /**
         * @brief Reconfigure transmitter components of satellite
         *
         * Supports reconfiguring of the following config parameters:
         * * `_bor_timeout`
         * * `_eor_timeout`
         * * `_data_timeout`
         *
         * @param partial_config Changes to the configuration of the satellite
         */
        void reconfiguring_transmitter(const config::Configuration& partial_config);

        /**
         * @brief Start transmitter components of satellite
         *
         * This functions sends the BOR message.
         *
         * @param run_identifier Run identifier for the upcoming run
         * @param config Configuration to send in BOR message
         * @throw SendTimeoutError If BOR send timeout is reached
         */
        void starting_transmitter(std::string_view run_identifier, const config::Configuration& config);

        /**
         * @brief Stop transmitter components of satellite and send the EOR
         *
         * @throw SendTimeoutError If EOR send timeout is reached
         */
        void stopping_transmitter();

        /**
         * @brief Interrupt function of transmitter
         *
         * If the previous state is RUN, this sends an EOR message marking the end of the run indicating an interruption.
         *
         * @throw SendTimeoutError If EOR send timeout is reached
         *
         * @param previous_state State in which the satellite was being interrupted
         */
        void interrupting_transmitter(protocol::CSCP::State previous_state);

        /**
         * @brief Set send timeout
         *
         * @param timeout Timeout, -1 is infinite (block until sent)
         */
        void set_send_timeout(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1));

        /**
         * @brief Send the EOR message
         *
         * @throw SendTimeoutError If EOR send timeout is reached
         */
        void send_eor();

        /**
         * @brief Set tag for the run metadata send as payload of the EOR message
         */
        template <typename T> void set_run_metadata_tag(std::string_view key, const T& value) {
            run_metadata_[utils::transform(key, ::tolower)] = value;
        }

    private:
        zmq::socket_t cdtp_push_socket_;
        networking::Port cdtp_port_;
        log::Logger cdtp_logger_;
        std::chrono::seconds data_bor_timeout_ {};
        std::chrono::seconds data_eor_timeout_ {};
        std::chrono::seconds data_msg_timeout_ {};
        std::uint64_t seq_ {};
        config::Dictionary bor_tags_;
        config::Dictionary eor_tags_;
        config::Dictionary run_metadata_;
        bool mark_run_tainted_ {false};
        std::atomic_size_t bytes_transmitted_;
    };

} // namespace constellation::satellite
