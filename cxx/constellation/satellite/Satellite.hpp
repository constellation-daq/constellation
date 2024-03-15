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

#include "constellation/core/config.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/satellite/fsm_definitions.hpp"

namespace constellation::satellite {

    class CNSTLN_API Satellite {
    public:
        virtual ~Satellite() = default;

        // No copy/move constructor/assignment
        Satellite(Satellite& other) = delete;
        Satellite& operator=(Satellite other) = delete;
        Satellite(Satellite&& other) = delete;
        Satellite& operator=(Satellite&& other) = delete;

    public:
        /**
         * Initialize satellite, e.g. check config, connect to device
         *
         * Note: do not perform actions that require actions before shutdown or another initialization
         *
         * @param stop_token Token which tracks if initializing should be aborted
         * @param config Configuration of the Satellite TODO(stephan.lachnit): proper class
         */
        virtual void initializing(const std::stop_token& stop_token, const std::any& config);

        /**
         * Launch satellite, i.e. apply configuration
         *
         * @param stop_token Token which tracks if launching should be aborted
         */
        virtual void launching(const std::stop_token& stop_token);

        /**
         * Landing satellite, i.e. undo what was done in the `launching` function
         *
         * @param stop_token Token which tracks if landing should be aborted
         */
        virtual void landing(const std::stop_token& stop_token);

        /**
         * Reconfigure satellite, i.e. apply a partial configuration to the already launched satellite
         *
         * @param stop_token Token which tracks if reconfiguring should be aborted
         * @param partial_config Changes to the configuration of the Satellite TODO(stephan.lachnit): proper class
         */
        virtual void reconfiguring(const std::stop_token& stop_token, const std::any& partial_config);

        /**
         * Start satellite, i.e. prepare for immediate data taking, e.g. opening files or creating buffers
         *
         * @param stop_token Token which tracks if starting should be aborted
         * @param run_number Run number for the upcoming run
         */
        virtual void starting(const std::stop_token& stop_token, std::uint32_t run_number);

        /**
         * Stop satellite, i.e. prepare to return to ORBIT state, e.g. closing open files
         *
         * @param stop_token Token which tracks if stopping should be aborted
         */
        virtual void stopping(const std::stop_token& stop_token);

        /**
         * Run function, i.e. run data acquisition
         *
         * @param stop_token Token which tracks if running should be stopped or aborted
         */
        virtual void running(const std::stop_token& stop_token);

        /**
         * Interrupt function, i.e. go immediately to safe state
         *
         * @param stop_token Token which tracks if interrupting should be aborted (i.e. an incoming shutdown)
         * @param previous_state State in which the Satellite was being interrupted
         */
        virtual void interrupting(const std::stop_token& stop_token, State previous_state);

        /**
         * On-Failure function, i.e. executed when entering the failure state
         *
         * @param stop_token Token which tracks if on_failure should be aborted (e.g. an incoming shutdown or restart)
         * @param previous_state State in which the Satellite was before experiencing a failure
         */
        virtual void on_failure(const std::stop_token& stop_token, State previous_state);

    public:
        /** Whether or not the reconfigure function is implemented */
        constexpr bool supportsReconfigure() const { return support_reconfigure_; }

        /** Return status of the satellite */
        std::string_view getStatus() const { return status_; }

        /** Return satellite name (class.device_name) */
        std::string_view getName() const { return name_; }

    protected:
        Satellite(std::string_view name);

        /** Enable or disable support for reconfigure transition (disabled by default) */
        constexpr void support_reconfigure(bool enable = true) { support_reconfigure_ = enable; }

        /** Set status of the satellite */
        void set_status(std::string status) { status_ = std::move(status); }

        /** Logger to use */
        log::Logger logger_; // NOLINT(*-non-private-member-variables-in-classes)

    private:
        bool support_reconfigure_ {false};
        std::string status_;
        std::string name_;
    };

    using Generator = std::shared_ptr<Satellite>(std::string_view);

} // namespace constellation::satellite
