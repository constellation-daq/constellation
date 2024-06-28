/**
 * @file
 * @brief Data receiver for a single satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include <zmq.hpp>

#include "constellation/build.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/core/message/CDTP1Message.hpp"

namespace constellation::data {
    class SingleDataReceiver {
    private:
        enum class State : std::uint8_t {
            BEFORE_BOR,
            IN_RUN,
            STOPPING,
            GOT_EOR,
        };

    public:
        /**
         * @brief Construct new data receiver for single sending satellite
         */
        CNSTLN_API SingleDataReceiver();

        /**
         * @brief Initialize data receiver
         *
         * Reads the following config parameters:
         * * `_data_sender_name`
         * * `_data_chirp_timeout`
         * * `_data_bor_timeout`
         * * `_data_data_timeout`
         * * `_data_eor_timeout`
         */
        CNSTLN_API void initializing(config::Configuration& config);

        /**
         * @brief Launch data receiver by finding the sending satellite via CHIRP
         *
         * @throw ChirpTimeoutError If the `_data_chirp_timeout` is reached
         */
        CNSTLN_API void launching();

        /**
         * @brief Start data receiver by receiving BOR with sending satellite's config
         *
         * @throw RecvTimeoutError If the `_data_bor_timeout` is reached
         */
        CNSTLN_API config::Dictionary starting();

        /**
         * @brief Receive the next data message
         *
         * If either the timeout is reached or the EOR is received, no message (empty optional) is returned.
         * The function can be called again if no message is returned.
         */
        CNSTLN_API std::optional<message::CDTP1Message> recvData();

        /**
         * @brief Stop data receiver to set receive timeout to EOR timeout
         *
         * @throw RecvTimeoutError If in `State::STOPPING` and `_data_eor_timeout` reached
         */
        CNSTLN_API void stopping();

        /**
         * @return If the EOR has been received
         */
        CNSTLN_API bool gotEOR();

        /**
         * @brief Get EOR message
         *
         * @return Dictionary with sending satellite's run metadata
         */
        CNSTLN_API const config::Dictionary& getEOR();

    private:
        /**
         * @brief Set receive timeout
         *
         * @param timeout Timeout, -1 is infinite (block until received)
         */
        void set_recv_timeout(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1));

    private:
        zmq::context_t context_;
        zmq::socket_t socket_;
        log::Logger logger_;
        State state_;
        std::string sender_name_;
        std::chrono::seconds data_chirp_timeout_ {};
        std::chrono::seconds data_bor_timeout_ {};
        std::chrono::seconds data_data_timeout_ {};
        std::chrono::seconds data_eor_timeout_ {};
        std::size_t seq_ {};
        std::string uri_;
        config::Dictionary eor_;
    };
} // namespace constellation::data
