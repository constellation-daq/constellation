/**
 * @file
 * @brief Heartbeat Manager
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>

#include "constellation/build.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/heartbeat/HeartbeatRecv.hpp"
#include "constellation/core/heartbeat/HeartbeatSend.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/message/CHP1Message.hpp"
#include "constellation/core/networking/Port.hpp"
#include "constellation/core/protocol/CHP_definitions.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/string_hash_map.hpp"

namespace constellation::heartbeat {

    /**
     * @brief Manager for CHP publishing and receiving
     *
     * This manager holds a heartbeat sender and receiver as well as the logic for calling FSM interrupts based on
     * received heartbeats. It keeps track of received heartbeats from remote heartbeat senders, counts their lives and
     * takes action either upon missing heartbeats or a remote ERROR state of the FSM.
     */
    class HeartbeatManager : public HeartbeatRecv {
    public:
        /**
         * @brief Construct a heartbeat manager
         *
         * The constructor directly starts sender and receiver as well as the manager's watchdog thread which keeps track
         * of remote heartbeat rates and states
         *
         * @param sender Canonical name of the heartbeat sender
         * @param state_callback Function that return the current state
         * @param interrupt_callback Interrupt callback which is invoked when a remote heartbeat sender reports an ERROR
         * state or stopped sending heartbeats
         */
        CNSTLN_API HeartbeatManager(std::string sender,
                                    std::function<protocol::CSCP::State()> state_callback,
                                    std::function<void(std::string_view)> interrupt_callback);

        /** Deconstruct the manager. This stops the watchdog thread */
        CNSTLN_API virtual ~HeartbeatManager();

        // No copy/move constructor/assignment
        /// @cond doxygen_suppress
        HeartbeatManager(const HeartbeatManager& other) = delete;
        HeartbeatManager& operator=(const HeartbeatManager& other) = delete;
        HeartbeatManager(HeartbeatManager&& other) = delete;
        HeartbeatManager& operator=(HeartbeatManager&& other) = delete;
        /// @endcond

        /**
         * @brief Send an extrasystole
         *
         * @param status Latest status message of the satellite
         */
        CNSTLN_API void sendExtrasystole(std::string_view status);

        /**
         * @brief Obtain the current state registered from a given remote
         *
         * @param remote Canonical name of the remote in question
         * @return Currently registered state of the remote if remote is present, empty optional otherwise
         */
        CNSTLN_API std::optional<protocol::CSCP::State> getRemoteState(std::string_view remote);

        /**
         * @brief Update the maximum heartbeat interval to a new value
         *
         * @note Heartbeats are send roughly twice as often as the maximum heartbeat interval
         *
         * @param interval New maximum heartbeat interval
         */
        CNSTLN_API void updateInterval(std::chrono::milliseconds interval) { sender_.updateInterval(interval); }

        /**
         * @brief Configure whether regular departures are allows.
         * @details If set to true, departing satellites which send a proper DEPART CHIRP message are not considered
         *          erroneous but are removed from the list of monitored heartbeats. If set to false, any missing heartbeat,
         *          even after a regular departure, is considered erroneous and the interruption callback is activated
         *
         * @param allow Boolean flag whether regular departures are allowed or not
         */
        CNSTLN_API void allowDeparture(bool allow) { allow_departure_ = allow; }

        /**
         * @brief Get ephemeral port to which the CHP socket is bound
         *
         * @return Port number
         */
        constexpr networking::Port getPort() const { return sender_.getPort(); }

    private:
        /**
         * @brief Helper to process heartbeats. This is registered as callback in the heartbeat receiver
         * @details It registers and updates the last heartbeat time point as well as the received state from remote
         * heartbeat services
         *
         * @param msg Received CHP message from remote service
         * */
        void process_heartbeat(const message::CHP1Message& msg);

        /**
         * @brief Helper to process satellite departure
         * @details Proper departure of satellites is considered different from the simple disappearance of a heartbeat
         * signal. Whether to not to take appropriate action can be configured.
         *
         * @param service The remote service which has departed and send an appropriate CHIRP DEPART message
         */
        void host_disconnected(const chirp::DiscoveredService& service) override;

        /**
         * @brief Main loop of the manager which checks for heartbeats of registered remotes.
         * @details The thread sleeps until the next remote is expected to have sent a heartbeat, checks if any of the
         * heartbeats are late or missing and goes back to sleep. This thread holds the main logic for autonomous operation,
         * the reaction to remote ERROR states and the counting of lives as specified by the CHP protocol.
         *
         * @param stop_token Stop token to interrupt the thread
         */
        void run(const std::stop_token& stop_token);

        /** Sender service */
        HeartbeatSend sender_;

        /** Interrupt callback invoked upon remote failure condition and missing heartbeats */
        std::function<void(std::string_view)> interrupt_callback_;

        /**
         * @struct Remote
         * @brief Struct holding all relevant information for a remote CHP host
         */
        struct Remote {
            // TODO(simonspa) add importance here
            std::chrono::milliseconds interval {};
            std::chrono::system_clock::time_point last_heartbeat;
            protocol::CSCP::State last_state {};
            std::chrono::system_clock::time_point last_checked;
            std::uint8_t lives {protocol::CHP::Lives};
        };

        /** Map of remotes this manager tracks */
        utils::string_hash_map<Remote> remotes_;
        std::mutex mutex_;
        std::condition_variable cv_;

        /** Configuration */
        bool allow_departure_ {true};

        log::Logger logger_;

        std::jthread watchdog_thread_;
    };
} // namespace constellation::heartbeat
