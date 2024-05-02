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

namespace constellation::controller {

    class CNSTLN_API Controller {
    protected:
        struct Connection {
            zmq::socket_t req;
            message::MD5Hash host_id;
        };

    public:
        virtual ~Controller() = default;

        // No copy/move constructor/assignment
        Controller(const Controller& other) = delete;
        Controller& operator=(const Controller& other) = delete;
        Controller(Controller&& other) = delete;
        Controller& operator=(Controller&& other) = delete;

    public:
        void registerSatellite(constellation::chirp::DiscoveredService service, bool depart, std::any user_data);

        message::CSCP1Message sendCommand(std::string_view satellite_name, message::CSCP1Message& cmd);

        std::map<std::string, message::CSCP1Message> sendCommand(message::CSCP1Message& cmd);

    private:
        message::CSCP1Message send_receive(Connection& conn, message::CSCP1Message& cmd);

    protected:
        Controller(std::string_view controller_name);

        /** Logger to use */
        log::Logger logger_; // NOLINT(*-non-private-member-variables-in-classes)

    private:
        std::string controller_name_;
        zmq::context_t context_ {};
        std::map<std::string, Connection, std::less<>> satellite_connections_;
    };

} // namespace constellation::controller
