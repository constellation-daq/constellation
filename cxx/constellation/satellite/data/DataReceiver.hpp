/**
 * @file
 * @brief Data receiver for satellites
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
    class DataReceiver {
    private:
        enum class State : std::uint8_t {
            BEFORE_BOR,
            IN_RUN,
            STOPPING,
            GOT_EOR,
        };

    public:
        CNSTLN_API DataReceiver();

        /** Initialize data receiver */
        CNSTLN_API void initializing(config::Configuration& config);

        /** Starting: receive BOR with sending satellite's config */
        CNSTLN_API config::Dictionary starting();

        /**
         * @brief Receive the next data message
         *
         * If either the timeout is reached or the EOR is received, no message is returned. The function can be called again
         * if no message is returned.
         */
        CNSTLN_API std::optional<message::CDTP1Message> recvData();

        /** Stopping: set receive timeout to EOR timeout */
        CNSTLN_API void stopping();

        /** Return if the EOR has been received */
        CNSTLN_API bool gotEOR();

        /** Return end of run: sending satellite's run metadata */
        CNSTLN_API const config::Dictionary& getEOR();

    private:
        /** Set receive timeout, -1 is infinite (block until received) */
        void set_recv_timeout(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1));

    private:
        zmq::context_t context_;
        zmq::socket_t socket_;
        log::Logger logger_;
        State state_;
        std::string sender_name_;
        std::chrono::seconds data_bor_timeout_ {};
        std::chrono::seconds data_data_timeout_ {};
        std::chrono::seconds data_eor_timeout_ {};
        std::size_t seq_ {};
        std::string uri_;
        config::Dictionary eor_;
    };
} // namespace constellation::data
