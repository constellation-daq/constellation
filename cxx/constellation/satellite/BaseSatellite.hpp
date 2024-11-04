/**
 * @file
 * @brief Base satellite with internal functionality
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
#include <tuple>
#include <utility>

#include <zmq.hpp>

#include "constellation/build.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/heartbeat/HeartbeatManager.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/networking.hpp"
#include "constellation/satellite/CommandRegistry.hpp"
#include "constellation/satellite/FSM.hpp"

namespace constellation::satellite {

    class CNSTLN_API BaseSatellite {
    protected:
        BaseSatellite(std::string_view type, std::string_view name);

    public:
        virtual ~BaseSatellite();

        // No copy/move constructor/assignment
        /// @cond doxygen_suppress
        BaseSatellite(const BaseSatellite& other) = delete;
        BaseSatellite& operator=(const BaseSatellite& other) = delete;
        BaseSatellite(BaseSatellite&& other) = delete;
        BaseSatellite& operator=(BaseSatellite&& other) = delete;
        /// @endcond

    private:
        // FSM needs access to internal functions
        friend FSM;

        // Satellite needs access to internal variables
        friend class Satellite;

    public:
        /**
         * @brief Return the satellite type
         */
        constexpr std::string_view getSatelliteType() const { return satellite_type_; }

        /**
         * @brief Return the satellite name
         */
        constexpr std::string_view getSatelliteName() const { return satellite_name_; }

        /**
         * @brief Return the canonical name of the satellite
         *
         * The canonical name is satellite_type.satellite_name
         */
        std::string getCanonicalName() const;

        /**
         * @brief Returns if online configuration is supported
         */
        constexpr bool supportsReconfigure() const { return support_reconfigure_; }

        /**
         * @brief Return current state of the satellite
         */
        protocol::CSCP::State getState() const { return fsm_.getState(); }

        /**
         * @brief Return the current status of the satellite
         */
        std::string_view getStatus() const { return status_; }

        /**
         * @brief Return the current or last used run identifier
         */
        std::string_view getRunIdentifier() const { return run_identifier_; }

        /**
         * @brief Return the ephemeral port number to which the CSCP socket is bound to
         */
        constexpr utils::Port getCommandPort() const { return cscp_port_; }

        /**
         * @brief Return the FSM of the satellite
         *
         * @warning Use carefully, the FSM gives direct access to low level functionality of the framework.
         */
        FSM& getFSM() { return fsm_; }

        /**
         * @brief Join CSCP processing thread
         *
         * Join the CSCP processing thread, which happens when the satellite is shut down or terminated.
         */
        void join();

        /**
         * @brief Terminate the satellite
         */
        void terminate();

    private:
        /**
         * @brief Get the next CSCP command
         *
         * @return CSCP message if any received
         */
        std::optional<message::CSCP1Message> get_next_command();

        /**
         * @brief Send a reply for a CSCP command
         *
         * @param reply_verb CSCP reply verb
         * @param payload Optional message payload
         * @param tags Header metadata
         */
        void send_reply(std::pair<message::CSCP1Message::Type, std::string> reply_verb,
                        message::PayloadBuffer payload = {},
                        config::Dictionary tags = {});

        /**
         * @brief Handle standard CSCP commands
         *
         * @param command Command string to try to handle as standard command
         * @return CSCP reply verb and message payload if a standard command
         */
        std::optional<
            std::tuple<std::pair<message::CSCP1Message::Type, std::string>, message::PayloadBuffer, config::Dictionary>>
        handle_standard_command(std::string_view command);

        /**
         * @brief Handle user CSCP commands
         *
         * @param command Command string to try to handle as user command
         * @param payload CSCP message payload sent with command
         * @return CSCP reply verb and message payload if a standard command
         */
        std::optional<std::pair<std::pair<message::CSCP1Message::Type, std::string>, message::PayloadBuffer>>
        handle_user_command(std::string_view command, const message::PayloadBuffer& payload);

        /**
         * @brief CSCP loop handling incoming CSCP messages
         *
         * @param stop_token Stop token to interrupt the thread
         */
        void cscp_loop(const std::stop_token& stop_token);

        /**
         * @brief Parse and apply internal parameters for the satellite from the configuration
         */
        void apply_internal_config(const config::Configuration& config);

        /**
         * @brief Store configuration in satellite
         */
        void store_config(config::Configuration&& config);

        /**
         * @brief Update configuration stored in satellite
         */
        void update_config(const config::Configuration& partial_config);

        /**
         * @brief Set a new status message
         */
        void set_status(std::string status) { status_ = std::move(status); }

    public:
        /// @cond doxygen_suppress
        virtual void initializing(config::Configuration& config) = 0;
        virtual void launching() = 0;
        virtual void landing() = 0;
        virtual void reconfiguring(const config::Configuration& partial_config) = 0;
        virtual void starting(std::string_view run_identifier) = 0;
        virtual void stopping() = 0;
        virtual void running(const std::stop_token& stop_token) = 0;
        virtual void interrupting(protocol::CSCP::State previous_state) = 0;
        virtual void failure(protocol::CSCP::State previous_state) = 0;
        /// @endcond

    private:
        void initializing_wrapper(config::Configuration&& config);
        void launching_wrapper();
        void landing_wrapper();
        void reconfiguring_wrapper(const config::Configuration& partial_config);
        void starting_wrapper(std::string run_identifier);
        void stopping_wrapper();
        void running_wrapper(const std::stop_token& stop_token);
        void interrupting_wrapper(protocol::CSCP::State previous_state);
        void failure_wrapper(protocol::CSCP::State previous_state);

    protected:
        log::Logger logger_; // NOLINT(misc-non-private-member-variables-in-classes)

    private:
        zmq::socket_t cscp_rep_socket_;
        utils::Port cscp_port_;

        std::string_view satellite_type_;
        std::string_view satellite_name_;
        FSM fsm_;

        log::Logger cscp_logger_;
        std::jthread cscp_thread_;
        bool support_reconfigure_ {false};
        std::string status_;
        config::Configuration config_;
        std::string run_identifier_;

        CommandRegistry user_commands_;

        heartbeat::HeartbeatManager heartbeat_manager_;
    };

} // namespace constellation::satellite
