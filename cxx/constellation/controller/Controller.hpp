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
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <variant>

#include <zmq.hpp>

#include "constellation/build.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/heartbeat/HeartbeatRecv.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/message/CHP1Message.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/protocol/CHP_definitions.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"

namespace constellation::controller {

    /** Controller base class which handles satellite connections, command distribution and heartbeating */
    class CNSTLN_API Controller {
    public:
        /** Payload of a command function: variant with (configuration) dictionary, (argument) list or (run id) string */
        using CommandPayload = std::variant<std::monostate, config::Dictionary, config::List, std::string>;

    protected:
        /** Update identifier */
        enum class UpdateType : std::uint8_t {
            /** Connection data has been updated */
            UPDATED,
            /** A connection has been added */
            ADDED,
            /** A connection has been removed */
            REMOVED,
        };

        /**
         * @struct Connection
         * @brief Local representation of a remote connection and state
         * @details Remote connection, comprising the socket and host ID and URI of a remote satellite as well as its last
         * known state, the last command response and verb. Furthermore, the current heartbeat interval, heartbeat check
         * time points and lives are kept.
         */
        struct Connection {
            /** Connection */
            zmq::socket_t req;
            message::MD5Hash host_id;
            std::string uri;

            /** State and last response */
            protocol::CSCP::State state {protocol::CSCP::State::NEW};
            message::CSCP1Message::Type last_cmd_type {};
            std::string last_cmd_verb {}; // NOLINT(readability-redundant-member-init)
            config::Dictionary commands {};

            /** Heartbeat status */
            std::chrono::milliseconds interval {10000};
            std::chrono::system_clock::time_point last_heartbeat {std::chrono::system_clock::now()};
            std::chrono::system_clock::time_point last_checked {std::chrono::system_clock::now()};
            std::uint8_t lives {protocol::CHP::Lives};
        };

    public:
        /**
         * @brief Construct a controller base object
         *
         * @param controller_name Name of the controller
         */
        Controller(std::string controller_name);

        /**
         * @brief Destruct controller
         *
         * @warning `stop()` has to be called before the pool can be safely destructed
         */
        virtual ~Controller() = default;

        /*
         * @brief This starts the heartbeat receiver thread, registers a CHIRP service discovery callback and sends a
         * CHIRP request beacon for CONTROL-type services
         */
        void start();

        /**
         * @brief Destruct the controller base class object
         * @details This deregisters the CHIRP service discovery callback and closes all open connections to satellites
         */
        void stop();

        /// @cond doxygen_suppress
        // No copy/move constructor/assignment
        Controller(const Controller& other) = delete;
        Controller& operator=(const Controller& other) = delete;
        Controller(Controller&& other) = delete;
        Controller& operator=(Controller&& other) = delete;
        /// @endcond

        /**
         * @brief Send a command to a single satellite
         * @details This method allows to send an already prepared command message to a connected satellite, identified via
         * its canonical name. Returns a message with verb ERROR if the satellite is not connected or the message is not a
         * request. If the command was successfully transmitted to the satellite, the response message of the command is
         * returned.
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
         * Returns a message with verb ERROR if the satellite is not connected. If the command was successfully transmitted
         * to the satellite, the response message of the command is returned.
         *
         * @param satellite_name Canonical name of the target satellite
         * @param verb Command
         * @param payload Optional payload for this command message
         *
         * @return CSCP response message
         */
        message::CSCP1Message sendCommand(std::string_view satellite_name,
                                          std::string verb,
                                          const CommandPayload& payload = {});

        /**
         * @brief Send a command to all connected satellites
         * @details This method allows to send an already prepared command message to all connected satellites. The response
         * from all satellites is returned as a map.
         *
         * @param cmd Command message
         *
         * @return Map of satellite canonical names and their CSCP response messages
         */
        std::map<std::string, message::CSCP1Message> sendCommands(message::CSCP1Message& cmd);

        /**
         * @brief Send a command to all connected satellites
         * @details This method allows to send command message to all connected satellites. The message is formed from the
         * provided verb and optional payload. The payload is the same for all satellites. The response from all satellites
         * is returned as a map.
         *
         * @param verb Command
         * @param payload Optional payload for this command message
         *
         * @return Map of satellite canonical names and their CSCP response messages
         */
        std::map<std::string, message::CSCP1Message> sendCommands(std::string verb, const CommandPayload& payload = {});

        /**
         * @brief Send a command to all connected satellites
         * @details This method allows to send command message to all connected satellites. The message is formed
         * individually for each satellite from the provided verb and the payload entry in the map for the given satellite.
         * Missing entries in the payload table will receive an empty payload. The response from all satellites is
         * returned as a map.
         *
         * @param verb Command
         * @param payloads Map of payloads for each target satellite.
         *
         * @return Map of satellite canonical names and their CSCP response messages
         */
        std::map<std::string, message::CSCP1Message> sendCommands(const std::string& verb,
                                                                  const std::map<std::string, CommandPayload>& payloads);

        /**
         * @brief Helper to check if all connected satellites are in a given state
         *
         * @param state State to be checked for
         * @return True if all connected satellites are in the given state, false otherwise
         */
        bool isInState(protocol::CSCP::State state) const;

        /**
         * @brief Helper to check if the constellation is in a coherent global state of if states are mixed
         *
         * @return True if all connected satellites are in the same state, false if states are mixed
         */
        bool isInGlobalState() const;

        /**
         * @brief Get lowest state of any satellite connected
         * @details This returns the lowest state of any of the satellites. Here, "lowest" refers to the state code, i.e. the
         * underlying value of the protocol::CSCP::State enum.
         * @return Lowest state currently held
         */
        protocol::CSCP::State getLowestState() const;

        /**
         * @brief Get set of currently active connected satellites
         * @return Set of fully-qualified canonical names of currently connected satellites
         */
        std::set<std::string> getConnections() const;

        /**
         * @brief Get total number of currently active connected satellites
         * @return Number of currently connected satellites
         */
        std::size_t getConnectionCount() const { return connection_count_.load(); }

        /**
         * @brief Return the current or last run identifier of the constellation
         * @details This function will search through all connected satellites and returns the first valid run identifier
         * found. The value will be empty if the satellites have just started or no satellite is connected.
         *
         * @return Run identifier
         */
        std::string getRunIdentifier();

        /**
         * @brief Return the starting time of the current or last run of the constellation
         * @details This function will go through all connected satellites and returns the latest run starting time found.
         * The optional will not hold a value if the satellites have just started or no satellite is connected.
         *
         * @return Optional with the run starting time
         */
        std::optional<std::chrono::system_clock::time_point> getRunStartTime();

    protected:
        /**
         * @brief Method called whenever a new global or lowest state has been reached
         * @details A global state is a situation when all connected satellites share a common state. The lowest state is
         * often used to convey the state of a Constellation when its constituents are in different states, i.e. the
         * Constellation is not in a global state. Whenever a new state of the Constellation is reached, e.g. by a state
         * update of a satellite or the joining or departing of a satellite, this method is called. This can e.g. be used to
         * emit signals for user interfaces or to trigger further actions.
         *
         * @param state The new global state of the constellation
         * @param global Flag indicating whether the new state is global or lowest
         */
        virtual void reached_state(protocol::CSCP::State state, bool global);

        /**
         * @brief Method to propagate updates of connection data
         * @details This virtual method can be overridden by derived controller classes in order to be informed about
         * data updates of the attached connections such as state changes and additions or deletions of connections.
         * The parameter position holds the position of the updated data row.
         *
         * @param type Type of the connection update performed
         * @param position Index of the connection which has received an update
         * @param total Total number of current connections
         */
        virtual void propagate_update(UpdateType type, std::size_t position, std::size_t total);

    private:
        /**
         * @brief Helper to send a message to a connected satellite and receive the response
         *
         * @param conn Target connection
         * @param cmd CSCP message
         * @param keep_payload Flag to indicate whether to release payload upon sending or not. This should be set to true
         * when sending the same command to multiple satellites. Defaults to false.
         *
         * @return CSCP response message
         */
        message::CSCP1Message send_receive(Connection& conn, message::CSCP1Message& cmd, bool keep_payload = false) const;

        /**
         * @brief Helper to build command message from verb and command payload
         *
         * @param verb Command
         * @param payload Optional payload for this command message
         *
         * @return CSCP REQUEST message to be send
         */
        message::CSCP1Message build_message(std::string verb, const CommandPayload& payload) const;

        /**
         * @brief Callback helper for CHIPR service discovery
         *
         * @param service Discovered service
         * @param status Enum indicating the status of the service (discovery, departure, death)
         * @param user_data Pointer to the base controller instance
         */
        static void callback(chirp::DiscoveredService service,
                             constellation::chirp::ServiceStatus status,
                             std::any user_data);

        /**
         * @brief Implementation of the CONTROL service discovery callback
         * @details This implements the callback which registers new satellites via their advertised CONTROL service. For
         * newly discovered services, it connects a socket to the satellite control endpoint and registers the connection.
         * For departures, it closes the connection and removes the connection entry.
         *
         * @param service Discovered control service
         * @param status Enum indicating the status of the service (discovery, departure, death)
         */
        void callback_impl(const constellation::chirp::DiscoveredService& service,
                           constellation::chirp::ServiceStatus status);

        /**
         * @brief Helper to process heartbeats. This is registered as callback in the heartbeat receiver
         * @details It registers and updates the last heartbeat time point as well as the received state from remote
         * heartbeat services
         *
         * @param msg Received CHP message from remote service
         * */
        void process_heartbeat(const message::CHP1Message& msg);

        /**
         * @brief Loop to keep track of heartbeats and remove dead connections from the list.
         * @details The thread sleeps until the next remote is expected to have sent a heartbeat, checks if any of the
         * heartbeats are late or missing and goes back to sleep.
         *
         * @param stop_token Stop token to interrupt the thread
         */
        void controller_loop(const std::stop_token& stop_token);

    protected:
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

        /** Heartbeat receiver module */
        constellation::heartbeat::HeartbeatRecv heartbeat_receiver_;

        std::atomic_size_t connection_count_;

        std::condition_variable_any cv_;
        std::jthread watchdog_thread_;
    };

} // namespace constellation::controller
