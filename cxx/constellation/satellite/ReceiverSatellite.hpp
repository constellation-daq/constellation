/**
 * @file
 * @brief Data receiving satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

#include <zmq.hpp>

#include "constellation/build.hpp"
#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/pools/BasePool.hpp"
#include "constellation/core/utils/string_hash_map.hpp"
#include "constellation/satellite/BaseSatellite.hpp"
#include "constellation/satellite/Satellite.hpp"

namespace constellation::satellite {
    /**
     * @brief Satellite class with additional functions to receive data
     */
    class CNSTLN_API ReceiverSatellite
        : public Satellite,
          private pools::BasePool<message::CDTP1Message, chirp::DATA, zmq::socket_type::pull> {
    private:
        enum class TransmitterState : std::uint8_t {
            NOT_CONNECTED,
            BOR_RECEIVED,
            EOR_RECEIVED,
        };
        struct TransmitterStateSeq {
            TransmitterState state;
            std::uint64_t seq;
        };

    protected:
        /**
         * @brief Construct a data receiving satellite
         *
         * @param type Satellite type
         * @param name Name of this satellite instance
         */
        ReceiverSatellite(std::string_view type, std::string_view name);

        /**
         * @brief Receive and handle Begin-of-Run (BOR) message
         *
         * @param header Header of the BOR message containing e.g. the sender name
         * @param config Configuration of the sending satellite to be stored
         */
        virtual void receive_bor(const message::CDTP1Message::Header& header, config::Configuration config) = 0;

        /**
         * @brief Receive and handle data message
         *
         * @note Any tags in the header should be stored next to the payload frames.
         *
         * @param data_message Data message containing the header and the payload
         */
        virtual void receive_data(message::CDTP1Message&& data_message) = 0;

        /**
         * @brief Receive and handle End-of-Run (EOR) message
         *
         * @param header Header of the EOR message containing e.g. the sender name
         * @param run_metadata Dictionary with run metata of the sending satellite to be stored
         */
        virtual void receive_eor(const message::CDTP1Message::Header& header, config::Dictionary run_metadata) = 0;

    public:
        /**
         * @brief Run function
         *
         * @note For data receiving satellites, this function must not be implemented. Instead `receive_bor()`,
                 `receive_data()` and `receive_eor()` have to be implemented.
         *
         * @param stop_token Token which tracks if running should be stopped or aborted
         */
        void running(const std::stop_token& stop_token) final;

    protected:
        /**
         * @brief Checks whether or not to connect to a discovered service
         *
         * @return True if service should be connected to, false otherwise
         */
        bool should_connect(const chirp::DiscoveredService& service) final;

    private:
        // Needs access to receiver specific functions
        friend BaseSatellite;

        /**
         * @brief Initialize receiver components of satellite
         *
         * Reads the following config parameters:
         * * `_eor_timeout`
         * * `_data_transmitters`
         *
         * @param config Configuration of the satellite
         */
        void initializing_receiver(config::Configuration& config);

        /**
         * @brief Reconfigure receiver components of satellite
         *
         * Supports reconfiguring of the following config parameters:
         * * `_eor_timeout`
         * * `_data_transmitters`
         *
         * @param partial_config Changes to the configuration of the satellite
         */
        void reconfiguring_receiver(const config::Configuration& partial_config);

        /**
         * @brief Start receiver components of satellite
         *
         * This functions starts the BasePool thread to receive CDTP messages.
         */
        void starting_receiver();

        /**
         * @brief Stop receiver components of satellite
         *
         * This function stops the BasePool thread and waits for the EOR messages.
         *
         * @throw RecvTimeoutError If EOR of sallites that send a BOR is not received after timeout
         */
        void stopping_receiver();

        /**
         * @brief Interrupt receiver components of satellite
         *
         * This function calls the `stopping_receiver()` method, but emits fake EOR messages instead of throwing.
         */
        void interrupting_receiver();

        /**
         * @brief Failure function for receiver components of satellite
         *
         * This function stops the BasePool thread.
         */
        void failure_receiver();

        /**
         * @brief Reset the states of all (configured) data transmitters to `NOT_CONNECTED`
         */
        void reset_data_transmitter_states();

        /**
         * @brief Callback function for BasePool to handle CDTP messages
         *
         * @param message Received CDTP message
         */
        void handle_cdtp_message(message::CDTP1Message&& message);

        /**
         * @brief Handle BOR message before passing it to `receive_bor()`
         *
         * @param bor_message Received CDTP BOR message
         * @throw InvalidCDTPMessageType If already a BOR received
         */
        void handle_bor_message(message::CDTP1Message bor_message);

        /**
         * @brief Handle DATA message before passing it to `receive_data()`
         *
         * @param data_message Received CDTP DATA message
         * @throw InvalidCDTPMessageType If no BOR received yet
         */
        void handle_data_message(message::CDTP1Message data_message);

        /**
         * @brief Handle EOR message before passing it to `receive_eor()`
         *
         * @param eor_message Received CDTP EOR message
         * @throw InvalidCDTPMessageType If no BOR received yet
         */
        void handle_eor_message(message::CDTP1Message eor_message);

    private:
        log::Logger cdtp_logger_;
        std::chrono::seconds data_eor_timeout_ {};
        std::vector<std::string> data_transmitters_;
        utils::string_hash_map<TransmitterStateSeq> data_transmitter_states_;
        std::mutex data_transmitter_states_mutex_;
        std::uint64_t seqs_missed_ {};
    };

} // namespace constellation::satellite
