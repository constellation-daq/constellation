/**
 * @file
 * @brief Controller class with connections
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <any>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

#include <msgpack.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/build.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/heartbeat/HeartbeatRecv.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/satellite/fsm_definitions.hpp"

namespace constellation::controller {

    /** Controller base class which handles connections and heartbeating */
    class CNSTLN_API Controller {
    public:
        /** Payload of a transition function: variant with config, partial_config or run_nr */
        using CommandPayload = std::variant<std::monostate, config::Dictionary, config::List, std::uint32_t>;

    protected:
        /**
         * Connection, comprising the socket and host ID of a remote satellite as well as its last known state and status
         * message
         */
        struct Connection {
            zmq::socket_t req;
            message::MD5Hash host_id;
            satellite::State state {satellite::State::NEW};
            std::string status {};
        };

    public:
        /**
         * @brief Construct a controller base object
         * @details This starts the heartbeat receiver thread, registers a CHIRP service discovery callback and sends a
         * CHIRP request beacon for CONTROl-type services
         *
         * @param controller_name Name of the controller
         */
        Controller(std::string_view controller_name);

        /**
         * @brief Destruct the controller base class object
         * @details This deregisters the CHIRP service discovery callback and closes all open connections to satellites
         */
        virtual ~Controller();

        // No copy/move constructor/assignment
        Controller(const Controller& other) = delete;
        Controller& operator=(const Controller& other) = delete;
        Controller(Controller&& other) = delete;
        Controller& operator=(Controller&& other) = delete;

        /**
         * @brief Send a command to a single satellite
         * @details This method allows to send an already prepared command message to a connected satellite, identified via
         * its canonical name. Returns a message with verb ERROR if the satellite is not connected or the message is not a
         * request.
         *
         * @param satellite_name Canonical name of the target satellite
         * @param cmd Command message
         *
         * @return CSCP response message
         */
        message::CSCP1Message sendCommand(std::string_view satellite_name, message::CSCP1Message& cmd);

        /**
         * @brief Send a command to a single satellite
         * @details This method allows to send a command to a connected satellite, identified via its canonical name.
         * Returns a message with verb ERROR if the satellite is not connected.
         *
         * @param satellite_name Canonical name of the target satellite
         * @param verb Command
         * @param payload Optional payload for this command message
         *
         * @return CSCP response message
         */
        message::CSCP1Message sendCommand(std::string_view satellite_name,
                                          const std::string& verb,
                                          const CommandPayload& payload = {});

        /**
         * @brief Send a command to all connected satellites
         * @details This method allows to send an already prepared command message to all connected satellites.
         *
         * @param cmd Command message
         *
         * @return Map of satellite canonical names and their CSCP response messages
         */
        std::map<std::string, message::CSCP1Message> sendCommand(message::CSCP1Message& cmd);

        /**
         * @brief Send a command to all connected satellites
         * @details This method allows to send an already prepared command message to all connected satellites.
         *
         * @param verb Command
         * @param payload Optional payload for this command message
         *
         * @return Map of satellite canonical names and their CSCP response messages
         */
        std::map<std::string, message::CSCP1Message> sendCommand(const std::string& verb,
                                                                 const CommandPayload& payload = {});

        /**
         * @brief Helper to check if all connections are in a given state
         *
         * @param state State to be checked for
         * @return True if all connections are in the given state, false otherwise
         */
        bool isInState(satellite::State state) const;

        /**
         * @brief Get lowest state of any satellite connected
         * @details This returns the lowest state of any of the satellites. Here, "lowest" refers to the state code.
         * @return Lowest state currently held
         */
        satellite::State getLowestState() const;

    private:
        /**
         * @brief Helper to send a message to a connect and receive the answer
         *
         * @param conn Target connection
         * @param cmd CSCP message
         * @param keep_payload Flag to indicate whether to release payload upon sending or not. This should be set to true
         * when sending the same command to multiple satellites. Defaults to false.
         *
         * @return CSCP response message
         */
        static message::CSCP1Message send_receive(Connection& conn, message::CSCP1Message& cmd, bool keep_payload = false);

        /**
         * @brief Callback helper for CHIPR service discovery
         *
         * @param service Discovered service
         * @param depart Boolean indicating departure
         * @param user_data Pointer to the base controller instance
         */
        static void callback(chirp::DiscoveredService service, bool depart, std::any user_data);

        /**
         * @brief Implementation of the service discovery callback
         * @details This implements the callback which registers new satellites via their advertised CONTROL service. For
         * newly discovered services, it connects a socket to the satellite control endpoint and registers the connection.
         * For departures, it closes the connection and removes the connection entry.
         *
         * @param service Discovered control service
         * @param depart Boolean indicating departure
         */
        void callback_impl(const constellation::chirp::DiscoveredService& service, bool depart);

        /**
         * @brief Helper to process heartbeats. This is registered as callback in the heartbeat receiver
         * @details It registers and updates the last heartbeat time point as well as the received state from remote
         * heartbeat services
         *
         * @param msg Received CHP message from remote service
         * */
        void process_heartbeat(const message::CHP1Message& msg);

    protected:
        /**
         * @brief Method to propagate updates of the connections
         * @details This virtual method can be overridden by derived controller classes in order to be informed about
         * updates of the attached connections such as new or departed connections as well as state and data changes
         */
        virtual void propagate_update(std::size_t /*connections*/) {};

        /** Logger to use */
        log::Logger logger_; // NOLINT(*-non-private-member-variables-in-classes)

        /** Map of open connections */
        std::map<std::string, Connection, std::less<>> connections_; // NOLINT(*-non-private-member-variables-in-classes)

        /**
         * Mutex for accessing the connection map
         *
         * @note: This is marked mutable since some derived controllers may need to lock this for read access to the
         * connection list in functions marked as const.
         */
        mutable std::mutex connection_mutex_; // NOLINT(*-non-private-member-variables-in-classes)

    private:
        /** Name of this controller */
        std::string controller_name_;
        /** ZMQ context */
        zmq::context_t context_ {};
        /** Heartbeat receiver module */
        constellation::heartbeat::HeartbeatRecv heartbeat_receiver_;
    };

} // namespace constellation::controller
