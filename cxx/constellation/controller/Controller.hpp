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
#include "constellation/core/logging/Logger.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/satellite/fsm_definitions.hpp"

namespace constellation::controller {

    class CNSTLN_API Controller {
    public:
        /** Payload of a transition function: variant with config, partial_config or run_nr */
        using CommandPayload = std::variant<std::monostate, config::Dictionary, config::List, std::uint32_t>;

    protected:
        struct Connection {
            zmq::socket_t req;
            message::MD5Hash host_id;
            satellite::State state {satellite::State::NEW};
            std::string status {};
        };

    public:
        virtual ~Controller();
        Controller(std::string_view controller_name);

        // No copy/move constructor/assignment
        Controller(const Controller& other) = delete;
        Controller& operator=(const Controller& other) = delete;
        Controller(Controller&& other) = delete;
        Controller& operator=(Controller&& other) = delete;

    public:
        message::CSCP1Message sendCommand(std::string_view satellite_name, message::CSCP1Message& cmd);
        message::CSCP1Message sendCommand(std::string_view satellite_name,
                                          const std::string& verb,
                                          const CommandPayload& payload = {});

        std::map<std::string, message::CSCP1Message> sendCommand(message::CSCP1Message& cmd);
        std::map<std::string, message::CSCP1Message> sendCommand(const std::string& verb,
                                                                 const CommandPayload& payload = {});

        /**
         * @brief Helper to check if all connections are in a given state
         *
         * @param state State to be checked for
         * @return True if all connections are in the given state, false otherwise
         */
        bool isInState(satellite::State state);

        satellite::State getLowestState();

    private:
        message::CSCP1Message send_receive(Connection& conn, message::CSCP1Message& cmd);

        static void callback(chirp::DiscoveredService service, bool depart, std::any user_data);

        void callback_impl(const constellation::chirp::DiscoveredService& service, bool depart);

    protected:
        /**
         * @brief Method to propagate updates of the connections
         * @details This virtual method can be overridden by derived controller classes in order to be informed about
         * updates of the attached connections such as new or departed connections as well as state and data changes
         *
         * @param connections Number of current connections
         */
        virtual void propagate_update(std::size_t /*unused*/) {};

        /** Logger to use */
        log::Logger logger_;                                         // NOLINT(*-non-private-member-variables-in-classes)
        std::map<std::string, Connection, std::less<>> connections_; // NOLINT(*-non-private-member-variables-in-classes)
        mutable std::mutex connection_mutex_;                        // NOLINT(*-non-private-member-variables-in-classes)

    private:
        std::string controller_name_;
        zmq::context_t context_ {};
    };

} // namespace constellation::controller
