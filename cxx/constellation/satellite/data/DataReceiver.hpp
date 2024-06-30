/**
 * @file
 * @brief Data receiver
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */
#pragma once

#include <any>
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <stop_token>
#include <thread>

#include "constellation/build.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/pools/BasePool.hpp"

namespace constellation::data {

    /**
     * Receiver class for data from multiple endpoints in a Constellation
     *
     * This class registers a CHIRP callback for data services, connects automatically to all available and appearing
     * services in the constellation, receives CDTP messages from remote satellites and forwards
     * them to a callback registered upon creation of the receiver
     */
    class DataRecv : public utils::BasePool<message::CDTP1Message> {
    private:
        enum class State : std::uint8_t {
            BEFORE_BOR,
            IN_RUN,
            STOPPING,
            GOT_EOR,
        };

    public:
        /**
         * @brief Construct data receiver
         *
         * @param callback Callback function pointer for received data messages
         */
        DataRecv()
            : BasePool<message::CDTP1Message>(
                  chirp::DATA, logger_, std::bind_front(&DataRecv::receive, this), zmq::socket_type::pull),
              logger_("DATA") {}

    private:
        virtual void receive(const message::CDTP1Message&) {};

    private:
        std::map<std::string, State> states_;
        log::Logger logger_;
    };
} // namespace constellation::data
