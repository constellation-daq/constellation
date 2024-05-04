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
#include "constellation/core/logging/Logger.hpp"
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
         * @param run_number Run number for the upcoming run
         */
        virtual void starting(std::uint32_t run_number);

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
        constexpr std::string_view getTypeName() const { return type_name_; }

        /** Return the name of the satellite */
        constexpr std::string_view getSatelliteName() const { return satellite_name_; }

        /** Return the canonical satellite name (type_name.satellite_name) */
        std::string getCanonicalName() const;

        /** Return a const reference to the satellite configuration */
        const config::Configuration& getConfig() const { return config_; }

        /** Return a reference to the satellite logger */
        log::Logger& getLogger() { return logger_; }

    protected:
        Satellite(std::string_view type_name, std::string_view satellite_name);

        /** Enable or disable support for reconfigure transition (disabled by default) */
        constexpr void support_reconfigure(bool enable = true) { support_reconfigure_ = enable; }

        /** Set status of the satellite */
        void set_status(std::string status) { status_ = std::move(status); }

        /** Logger to use */
        log::Logger logger_; // NOLINT(*-non-private-member-variables-in-classes)

    private:
        // FSM needs access to configuration
        friend FSM;

        /** Store configuration in satellite */
        void store_config(config::Configuration&& config);

        /** Updated configuration stored in satellite */
        void update_config(const config::Configuration& partial_config);

    private:
        bool support_reconfigure_ {false};
        std::string status_;
        std::string_view type_name_;
        std::string_view satellite_name_;
        config::Configuration config_;
    };

    // Generator function that needs to be exported in a satellite library
    using Generator = std::shared_ptr<Satellite>(std::string_view, std::string_view);

} // namespace constellation::satellite
