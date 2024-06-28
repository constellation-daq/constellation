/**
 * @file
 * @brief Satellite class with user functions
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

#include "constellation/build.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/config/Value.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/satellite/CommandRegistry.hpp"
#include "constellation/satellite/fsm_definitions.hpp"

namespace constellation::satellite {

    class FSM;

    class CNSTLN_API Satellite {
    public:
        virtual ~Satellite() = default;

        // No copy/move constructor/assignment
        Satellite(const Satellite& other) = delete;
        Satellite& operator=(const Satellite& other) = delete;
        Satellite(Satellite&& other) = delete;
        Satellite& operator=(Satellite&& other) = delete;

    public:
        /**
         * Initialize satellite, e.g. check config, connect to device
         *
         * Note: do not perform actions that require actions before shutdown or another initialization
         *
         * @param config Configuration of the Satellite
         */
        virtual void initializing(config::Configuration& config);

        /**
         * Launch satellite, i.e. apply configuration
         */
        virtual void launching();

        /**
         * Landing satellite, i.e. undo what was done in the `launching` function
         */
        virtual void landing();

        /**
         * Reconfigure satellite, i.e. apply a partial configuration to the already launched satellite
         *
         * @param partial_config Changes to the configuration of the Satellite
         */
        virtual void reconfiguring(const config::Configuration& partial_config);

        /**
         * Start satellite, i.e. prepare for immediate data taking, e.g. opening files or creating buffers
         *
         * @param run_identifier Run identifier for the upcoming run
         */
        virtual void starting(std::string_view run_identifier);

        /**
         * Stop satellite, i.e. prepare to return to ORBIT state, e.g. closing open files
         */
        virtual void stopping();

        /**
         * Run function, i.e. run data acquisition
         *
         * @param stop_token Token which tracks if running should be stopped or aborted
         */
        virtual void running(const std::stop_token& stop_token);

        /**
         * Interrupt function, i.e. go immediately from ORBIT or RUN to SAFE state
         *
         * By default, this function calls `stopping` (if in RUN state) and then `landing`.
         *
         * @param previous_state State in which the Satellite was being interrupted
         */
        virtual void interrupting(State previous_state);

        /**
         * On-Failure function, i.e. executed when entering the ERROR state
         *
         * @param previous_state State in which the Satellite was before experiencing a failure
         */
        virtual void on_failure(State previous_state);

    public:
        /** Whether or not the reconfigure function is implemented */
        constexpr bool supportsReconfigure() const { return support_reconfigure_; }

        /** Return status of the satellite */
        std::string_view getStatus() const { return status_; }

        /** Return the name of the satellite type */
        constexpr std::string_view getType() const { return satellite_type_; }

        /** Return the name of the satellite */
        constexpr std::string_view getSatelliteName() const { return satellite_name_; }

        /** Return the canonical satellite name (type_name.satellite_name) */
        std::string getCanonicalName() const;

        /**
         * @brief Call a user-registered command
         *
         * @param state Current state of the satellite finite state machine
         * @param name Name of the command to be called
         * @param args List with arguments for the command
         * @return Return value of the command
         */
        config::Value callUserCommand(State state, const std::string& name, const config::List& args) {
            return user_commands_.call(state, name, args);
        }

        /** Return map of user-registered commands with their description */
        std::map<std::string, std::string> getUserCommands() const { return user_commands_.describeCommands(); }

        /** Return a const reference to the satellite configuration */
        const config::Configuration& getConfig() const { return config_; }

        /** Return the current run identifier */
        std::string_view getRunIdentifier() const { return run_identifier_; }

    protected:
        /**
         * @brief Construct a satellite
         *
         * @param type Satellite type
         * @param name Name of this satellite instance
         */
        Satellite(std::string_view type, std::string_view name);

        /** Enable or disable support for reconfigure transition (disabled by default) */
        constexpr void support_reconfigure(bool enable = true) { support_reconfigure_ = enable; }

        /** Set status of the satellite */
        void set_status(std::string status) { status_ = std::move(status); }

        /**
         * @brief Register a new user command
         *
         * @param name Name of the command
         * @param description Comprehensive description of the command
         * @param states States of the finite state machine in which this command can be called
         * @param func Pointer to the member function to be called
         * @param t Pointer to the satellite object
         */
        template <typename T, typename R, typename... Args>
        void register_command(const std::string& name,
                              std::string description,
                              std::initializer_list<State> states,
                              R (T::*func)(Args...),
                              T* t) {
            user_commands_.add(name, std::move(description), states, func, t);
        }

        /**
         * @brief Register a new user command from a function or lambda
         *
         * @param name Name of the command
         * @param description Comprehensive description of the command
         * @param states States of the finite state machine in which this command can be called
         * @param func Function to be called
         */
        template <typename R, typename... Args>
        void register_command(const std::string& name,
                              std::string description,
                              std::initializer_list<State> states,
                              std::function<R(Args...)> func) {
            user_commands_.add(name, std::move(description), states, func);
        }

    private:
        // FSM needs access to configuration
        friend FSM;

        /** Store configuration in satellite */
        void store_config(config::Configuration&& config);

        /** Updated configuration stored in satellite */
        void update_config(const config::Configuration& partial_config);

        /** Update the run identifier */
        void update_run_identifier(std::string_view run_identifier) { run_identifier_ = run_identifier; }

    private:
        /** Logger to use */
        log::Logger logger_;

        bool support_reconfigure_ {false};
        std::string status_;
        std::string_view satellite_type_;
        std::string_view satellite_name_;
        config::Configuration config_;
        std::string run_identifier_ {};
        CommandRegistry user_commands_;
    };

    // Generator function that needs to be exported in a satellite library
    using Generator = std::shared_ptr<Satellite>(std::string_view, std::string_view);

} // namespace constellation::satellite
