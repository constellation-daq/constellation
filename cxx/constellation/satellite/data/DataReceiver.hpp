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
    class CNSTLN_API DataRecv : public utils::BasePool<message::CDTP1Message> {
    private:
        enum class State : std::uint8_t {
            AWAITING_BOR,
            AWAITING_DATA,
        };

    public:
        /**
         * @brief Construct data receiver
         *
         * @param callback Callback function pointer for received data messages
         */
        DataRecv();
        ~DataRecv() = default;
        virtual void receive(const message::CDTP1Message&) {};

    private:
        void socket_connected(zmq::socket_t& socket) final;

        void receive_impl(const message::CDTP1Message&);

    private:
        struct Sender {
            State state;
            std::size_t seq;
        };

        std::map<std::string, Sender, std::less<>> states_;
        log::Logger logger_;
    };
} // namespace constellation::data
