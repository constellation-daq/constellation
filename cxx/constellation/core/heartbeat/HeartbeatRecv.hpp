/**
 * @file
 * @brief Heartbeat receiver
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

#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/build.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/message/CHP1Message.hpp"
#include "constellation/core/subscriber/Subscriber.hpp"

namespace constellation::heartbeat {

    /**
     * Receiver class for distributed heartbeats in a constellation
     *
     * This class registers a CHIRP callback for heartbeat services, subscribes automatically to all available and appearing
     * services in the constellation and listens for heartbeat and extrasystole messages from remote satellites and forwards
     * them to a callback registered upon creation of the receiver
     */
    class HeartbeatRecv : public Subscriber<message::CHP1Message> {
    public:
        /**
         * @brief Construct heartbeat receiver
         *
         * @param callback Callback function pointer for received heartbeat messages
         */
        CNSTLN_API HeartbeatRecv(std::function<void(const message::CHP1Message&)> callback)
            : Subscriber<message::CHP1Message>(chirp::HEARTBEAT, "CHP", callback, {""}) {}

        /** Default destructor */
        CNSTLN_API ~HeartbeatRecv() = default;
    };
} // namespace constellation::heartbeat
