/**
 * @file
 * @brief Heartbeat receiver
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */
#pragma once

#include <functional>
#include <utility>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/message/CHP1Message.hpp"
#include "constellation/core/pools/SubscriberPool.hpp"

namespace constellation::heartbeat {

    /**
     * @brief Receiver class for distributed heartbeats in a constellation
     *
     * This class registers a CHIRP callback for heartbeat services, subscribes automatically to all available and appearing
     * services in the constellation and listens for heartbeat and extrasystole messages from remote satellites and forwards
     * them to a callback registered upon creation of the receiver.
     *
     * @note Needs to be started with `start()` and stopped with `stop()`
     */
    class HeartbeatRecv : public pools::SubscriberPool<message::CHP1Message, chirp::HEARTBEAT> {
    public:
        /**
         * @brief Construct heartbeat receiver
         *
         * @param callback Callback function pointer for received heartbeat messages
         */
        HeartbeatRecv(std::function<void(message::CHP1Message&&)> callback)
            : SubscriberPool<message::CHP1Message, chirp::HEARTBEAT>("CHP", std::move(callback)) {}

    private:
        void host_connected(const chirp::DiscoveredService& service) final {
            // CHP: subscribe to all topics
            subscribe(service.host_id, "");
        }
    };
} // namespace constellation::heartbeat
