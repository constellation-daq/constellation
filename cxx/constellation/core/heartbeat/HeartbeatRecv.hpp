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
#include "constellation/core/logging/Logger.hpp"
#include "constellation/core/message/CHP1Message.hpp"
#include "constellation/core/message/exceptions.hpp"

namespace constellation::heartbeat {

    /**
     * Receiver class for distributed heartbeats in a constellation
     *
     * This class registers a CHIRP callback for heartbeat services, subscribes automatically to all available and appearing
     * services in the constellation and listens for heartbeat and extrasystole messages from remote satellites and forwards
     * them to a callback registered upon creation of the receiver
     */
    class HeartbeatRecv {
    public:
        /**
         * @brief Construct heartbeat receiver
         *
         * @param callback Callback function pointer for received heartbeat messages
         */
        CNSTLN_API HeartbeatRecv(std::function<void(const message::CHP1Message&)> callback);

        /**
         * @brief Destruct heartbeat receiver
         *
         * This closes all connections and unregisters the CHIRP service discovery callback
         */
        CNSTLN_API ~HeartbeatRecv();

        // No copy/move constructor/assignment
        HeartbeatRecv(const HeartbeatRecv& other) = delete;
        HeartbeatRecv& operator=(const HeartbeatRecv& other) = delete;
        HeartbeatRecv(HeartbeatRecv&& other) = delete;
        HeartbeatRecv& operator=(HeartbeatRecv&& other) = delete;

        /**
         * @brief Callback for CHIRP service discovery
         * @details This callback is run for every heartbeat service service discovered on or departing from the
         * constellation. It will connect or disconnect the remote heartbeat service and register the corresponding socket
         * with the central polling mechanism
         *
         * @param service Discovered service
         * @param depart Boolean to indicate discovery or departure
         * @param user_data Pointer to the heartbeat receiver instance
         */
        CNSTLN_API static void callback(chirp::DiscoveredService service, bool depart, std::any user_data);

    private:
        /** Callback implementation */
        void callback_impl(const chirp::DiscoveredService& service, bool depart);

        /** Main loop polling the registered sockets */
        void loop(const std::stop_token& stop_token);

        /** Helper to connect to a newly discovered socket */
        void connect(const chirp::DiscoveredService& service);

        /** Helper to disconnect from a departing heartbeat service */
        void disconnect(const chirp::DiscoveredService& service);

        /** Disconnect from all registered heartbeat services */
        void disconnect_all();

        log::Logger logger_;
        zmq::context_t context_;
        zmq::active_poller_t poller_;
        std::map<chirp::DiscoveredService, zmq::socket_t> sockets_;

        std::timed_mutex sockets_mutex_;
        std::atomic_flag af_;
        std::jthread receiver_thread_;

        std::function<void(const message::CHP1Message&)> message_callback_;
    };
} // namespace constellation::heartbeat
