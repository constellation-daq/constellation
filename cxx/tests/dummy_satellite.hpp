/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <stop_token>
#include <string_view>
#include <thread>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/logging/log.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/FSM.hpp"
#include "constellation/satellite/fsm_definitions.hpp"
#include "constellation/satellite/Satellite.hpp"

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class DummySatellite : public constellation::satellite::Satellite {
public:
    DummySatellite() : Satellite("Dummy", "sat1") {
        support_reconfigure();
        register_command("my_cmd", "A User Command", {}, &DummySatellite::usr_cmd, this);
        register_command("my_cmd_arg", "Another User Command", {}, &DummySatellite::usr_cmd_arg, this);
        register_command("my_cmd_invalid_return", "Invalid User Command", {}, &DummySatellite::usr_cmd_invalid_return, this);
        register_command("my_cmd_void", "Command without arguments & return", {}, &DummySatellite::usr_cmd_void, this);
        register_command("my_cmd_state",
                         "Command for RUN state only",
                         {constellation::satellite::State::RUN},
                         &DummySatellite::usr_cmd_void,
                         this);
    }

    void progressFsm() {
        auto old_state = getState();
        LOG(DEBUG) << "Progressing FSM, old state " << constellation::utils::to_string(old_state);
        progress_fsm_ = true;
        // wait for state change
        while(old_state == getState()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        progress_fsm_ = false;
        LOG(DEBUG) << "Progressed FSM, new state" << constellation::utils::to_string(getState());
    }

    void setSupportReconfigure(bool support_reconfigure) { Satellite::support_reconfigure(support_reconfigure); }
    void setThrowTransitional() { throw_transitional_ = true; }

    void initializing(constellation::config::Configuration& config) override {
        Satellite::initializing(config);
        transitional_state();
    }
    void launching() override {
        Satellite::launching();
        transitional_state();
    }
    void landing() override {
        Satellite::landing();
        transitional_state();
    }
    void reconfiguring(const constellation::config::Configuration& partial_config) override {
        Satellite::reconfiguring(partial_config);
        transitional_state();
    }
    void starting(std::string_view run_identifier) override {
        Satellite::starting(run_identifier);
        transitional_state();
    }
    void stopping() override {
        Satellite::stopping();
        transitional_state();
    }
    void running(const std::stop_token& stop_token) override {
        Satellite::running(stop_token);
        transitional_state();
    }
    void interrupting(constellation::satellite::State previous_state) override {
        // Note: the default implementation calls `stopping()` and `landing()`, both of which call `transitional_state()`
        progress_fsm_ = true;
        Satellite::interrupting(previous_state);
        progress_fsm_ = false;
        transitional_state();
    }
    void failure(constellation::satellite::State previous_state) override { Satellite::failure(previous_state); }

private:
    void transitional_state() {
        while(!progress_fsm_) {
            if(throw_transitional_) {
                throw_transitional_ = false;
                throw constellation::utils::Exception("Throwing in transitional state as requested");
            }
        }
    }

    // NOLINTBEGIN(readability-convert-member-functions-to-static,readability-make-member-function-const)
    int usr_cmd() { return 2; }
    int usr_cmd_arg(int a) { return 2 * a; }
    std::array<int, 1> usr_cmd_invalid_return() { return {2}; }
    void usr_cmd_void() { value_ = 3; };
    int value_ {2};
    // NOLINTEND(readability-convert-member-functions-to-static,readability-make-member-function-const)

private:
    std::atomic_bool progress_fsm_ {false};
    std::atomic_bool throw_transitional_ {false};
};
