/**
 * @file
 * @brief Satellite implementation
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "constellation/core/config.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/utils/ports.hpp"
#include "constellation/satellite/FSM.hpp"
#include "constellation/satellite/Satellite.hpp"

namespace constellation::satellite {

    class SatelliteImplementation final {
    public:
        CNSTLN_API SatelliteImplementation(std::shared_ptr<Satellite> satellite);

        CNSTLN_API ~SatelliteImplementation();

        // No copy/move constructor/assignment
        SatelliteImplementation(SatelliteImplementation& other) = delete;
        SatelliteImplementation& operator=(SatelliteImplementation other) = delete;
        SatelliteImplementation(SatelliteImplementation&& other) = delete;
        SatelliteImplementation& operator=(SatelliteImplementation&& other) = delete;

        /**
         * Get ephemeral port to which the CSCP socket is bound
         *
         * @return Port number
         */
        constexpr utils::Port getPort() const { return port_; }

        // start main_loop
        CNSTLN_API void start();

        // join main_loop
        CNSTLN_API void join();

    private:
        // get next command
        std::optional<message::CSCP1Message> getNextCommand();

        // reply to command
        void sendReply(std::pair<message::CSCP1Message::Type, std::string> reply_verb);

        // handle get commands
        std::optional<std::pair<message::CSCP1Message::Type, std::string>> handleGetCommand(std::string_view command);

        // main loop
        void main_loop(const std::stop_token& stop_token);

    private:
        zmq::context_t context_;
        zmq::socket_t rep_;
        utils::Port port_;
        std::shared_ptr<Satellite> satellite_;
        FSM fsm_;
        log::Logger logger_;
        std::jthread main_thread_;
    };

} // namespace constellation::satellite
