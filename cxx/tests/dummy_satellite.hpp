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
#include <utility>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/FSM.hpp"
#include "constellation/satellite/Satellite.hpp"

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
template <class SatelliteT = constellation::satellite::Satellite> class DummySatelliteNR : public SatelliteT {
public:
    DummySatelliteNR(std::string_view name = "sat1") : SatelliteT("Dummy", name) {
        SatelliteT::support_reconfigure();
        SatelliteT::register_command("my_cmd", "A User Command", {}, &DummySatelliteNR::usr_cmd, this);
        SatelliteT::register_command("my_cmd_arg", "Another User Command", {}, &DummySatelliteNR::usr_cmd_arg, this);
        SatelliteT::register_command(
            "my_cmd_invalid_return", "Invalid User Command", {}, &DummySatelliteNR::usr_cmd_invalid_return, this);
        SatelliteT::register_command(
            "my_cmd_void", "Command without arguments & return", {}, &DummySatelliteNR::usr_cmd_void, this);
        SatelliteT::register_command("my_cmd_state",
                                     "Command for RUN state only",
                                     {constellation::protocol::CSCP::State::RUN},
                                     &DummySatelliteNR::usr_cmd_void,
                                     this);
    }

    void reactFSM(constellation::satellite::FSM::Transition transition,
                  constellation::satellite::FSM::TransitionPayload payload = {},
                  bool progress = true) {
        SatelliteT::getFSM().react(transition, std::move(payload));
        if(progress) {
            progressFsm();
        }
    }

    void progressFsm() {
        auto old_state = SatelliteT::getState();
        LOG(DEBUG) << "Progressing FSM, old state " << constellation::utils::to_string(old_state);
        progress_fsm_ = true;
        // wait for state change
        while(old_state == SatelliteT::getState()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        progress_fsm_ = false;
        LOG(DEBUG) << "Progressed FSM, new state " << constellation::utils::to_string(SatelliteT::getState());
    }

    void setSupportReconfigure(bool support_reconfigure) { SatelliteT::support_reconfigure(support_reconfigure); }
    void setThrowTransitional() { throw_transitional_ = true; }

    void initializing(constellation::config::Configuration& config) override {
        SatelliteT::initializing(config);
        transitional_state();
    }
    void launching() override {
        SatelliteT::launching();
        transitional_state();
    }
    void landing() override {
        SatelliteT::landing();
        transitional_state();
    }
    void reconfiguring(const constellation::config::Configuration& partial_config) override {
        SatelliteT::reconfiguring(partial_config);
        transitional_state();
    }
    void starting(std::string_view run_identifier) override {
        SatelliteT::starting(run_identifier);
        transitional_state();
    }
    void stopping() override {
        SatelliteT::stopping();
        transitional_state();
    }
    void interrupting(constellation::protocol::CSCP::State previous_state) override {
        // Note: the default implementation calls `stopping()` and `landing()`, both of which call `transitional_state()`
        progress_fsm_ = true;
        SatelliteT::interrupting(previous_state);
        progress_fsm_ = false;
        transitional_state();
    }
    void failure(constellation::protocol::CSCP::State previous_state) override { SatelliteT::failure(previous_state); }

protected:
    void transitional_state() {
        LOG(TRACE) << "Entering transitional state";
        while(!progress_fsm_) {
            if(throw_transitional_) {
                throw_transitional_ = false;
                throw constellation::utils::Exception("Throwing in transitional state as requested");
            }
        }
        LOG(TRACE) << "Leaving transitional state";
    }

private:
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

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
template <class SatelliteT = constellation::satellite::Satellite>
class DummySatellite : public DummySatelliteNR<SatelliteT> {
public:
    DummySatellite(std::string_view name = "sat1") : DummySatelliteNR<SatelliteT>(name) {}

    void running(const std::stop_token& stop_token) override {
        SatelliteT::running(stop_token);
        DummySatelliteNR<SatelliteT>::transitional_state();
    }
};
