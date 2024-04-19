/**
 * @file
 * @brief FSM class
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "FSM.hpp"

#include <cstdint>
#include <exception>
#include <memory>
#include <stop_token>
#include <string>
#include <thread>
#include <typeinfo>
#include <utility>

#include <msgpack.hpp>
#include <zmq.hpp>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/logging/log.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/satellite/exceptions.hpp"
#include "constellation/satellite/fsm_definitions.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::satellite;
using namespace constellation::utils;
using namespace std::literals::string_literals;

FSM::~FSM() {
    run_thread_.request_stop();
    if(run_thread_.joinable()) {
        run_thread_.join();
    }
    if(transitional_thread_.joinable()) {
        transitional_thread_.join();
    }
    if(failure_thread_.joinable()) {
        failure_thread_.join();
    }
}

FSM::TransitionFunction FSM::findTransitionFunction(Transition transition) {
    // Get transition map for current state (never throws due to FSM design)
    const auto& transition_map = state_transition_map_.at(state_);
    // Find transition
    const auto transition_function_it = transition_map.find(transition);
    if(transition_function_it != transition_map.end()) [[likely]] {
        return transition_function_it->second;
    } else {
        throw FSMError("Transition " + to_string(transition) + " not allowed from " + to_string(state_) + " state");
    }
}

bool FSM::isAllowed(Transition transition) {
    try {
        findTransitionFunction(transition);
    } catch(const FSMError&) {
        return false;
    }
    return true;
}

void FSM::react(Transition transition, TransitionPayload payload) {
    // Find transition
    LOG(logger_, INFO) << "Reacting to transition " << to_string(transition);
    auto transition_function = findTransitionFunction(transition);
    // Execute transition function
    state_ = (this->*transition_function)(std::move(payload));
    LOG(logger_, STATUS) << "New state: " << to_string(state_);
}

bool FSM::reactIfAllowed(Transition transition, TransitionPayload payload) {
    try {
        react(transition, std::move(payload));
    } catch(const FSMError&) {
        LOG(logger_, INFO) << "Skipping transition " << to_string(transition);
        return false;
    }
    return true;
}

std::pair<CSCP1Message::Type, std::string> FSM::reactCommand(TransitionCommand transition_command,
                                                             const std::shared_ptr<zmq::message_t>& payload) {
    // Cast to normal transition, underlying values are identical
    auto transition = static_cast<Transition>(transition_command);
    LOG(logger_, INFO) << "Reacting to transition " << to_string(transition);
    // Check if command is a valid transition for the current state
    TransitionFunction transition_function {};
    try {
        transition_function = findTransitionFunction(transition);
    } catch(const FSMError& error) {
        LOG(logger_, WARNING) << error.what();
        return {CSCP1Message::Type::INVALID, error.what()};
    }
    // Check if reconfigure command is implemented in case it is requested
    if(transition == Transition::reconfigure && !satellite_->supportsReconfigure()) {
        std::string reconfigure_info {"Transition reconfigure is not implemented by this satellite"};
        LOG(logger_, WARNING) << reconfigure_info;
        return {CSCP1Message::Type::NOTIMPLEMENTED, std::move(reconfigure_info)};
    }
    // Check if payload only in initialize, reconfigure, and start
    auto should_have_payload =
        (transition == Transition::initialize || transition == Transition::reconfigure || transition == Transition::start);
    if(should_have_payload && !payload) {
        std::string payload_info {"Transition " + to_string(transition) + " requires a payload frame"};
        LOG(logger_, WARNING) << payload_info;
        return {CSCP1Message::Type::INCOMPLETE, std::move(payload_info)};
    }
    // If there is a payload, but it is not used add a note in the reply
    const std::string payload_note = (!should_have_payload && payload) ? " (payload frame is ignored)"s : ""s;

    // Try to decode the payload:
    TransitionPayload fsm_payload {};
    try {
        if(payload && !payload->empty()) {
            if(transition == Transition::initialize || transition == Transition::reconfigure) {
                const auto msgpack_payload = msgpack::unpack(utils::to_char_ptr(payload->data()), payload->size());
                fsm_payload = Configuration(msgpack_payload->as<Dictionary>());
            } else if(transition == Transition::start) {
                const auto msgpack_payload = msgpack::unpack(utils::to_char_ptr(payload->data()), payload->size());
                fsm_payload = msgpack_payload->as<std::uint32_t>();
            }
        }
    } catch(msgpack::unpack_error& e) {
        std::string payload_info {"Transition " + to_string(transition) + " received invalid payload: " + e.what()};
        LOG(logger_, WARNING) << payload_info;
        return {CSCP1Message::Type::INCOMPLETE, std::move(payload_info)};
    } catch(std::bad_cast&) {
        std::string payload_info {"Transition " + to_string(transition) + " received incorrect payload"};
        LOG(logger_, WARNING) << payload_info;
        return {CSCP1Message::Type::INCOMPLETE, std::move(payload_info)};
    }

    // Execute transition function
    state_ = (this->*transition_function)(std::move(fsm_payload));
    LOG(logger_, STATUS) << "New state: " << to_string(state_);

    // Return that command is being executed
    return {CSCP1Message::Type::SUCCESS, "Transition " + to_string(transition) + " is being initiated" + payload_note};
}

void FSM::interrupt() {
    LOG(logger_, STATUS) << "Interrupting...";
    //  Wait until we are in a steady state
    while(!is_steady(state_)) {
        LOG_ONCE(logger_, DEBUG) << "Waiting for a steady state...";
    }
    // In a steady state, try to react to interrupt
    reactIfAllowed(Transition::interrupt);
    // We could be in interrupting, so wait for steady state
    while(!is_steady(state_)) {
        LOG_ONCE(logger_, DEBUG) << "Waiting for a steady state...";
    }
}

// Calls the transition function of a satellite and return success transition if completed or failure on exception
template <typename Func, typename... Args>
Transition call_satellite_function(Satellite* satellite, Func func, Transition success_transition, Args... args) {
    try {
        // Call transition function of satellite
        (satellite->*func)(args...);
        // Finish transition
        return success_transition;
    } catch(const std::exception& error) {
        // Something went wrong, log and go to error state
        LOG(satellite->getLogger(), CRITICAL) << "Critical failure during transition: " << error.what();
        return Transition::failure;
    } catch(...) {
        // Something went wrong but not with a proper exception, go to error state
        return Transition::failure;
    }
}

// Joins a thread and assigns it to a new thread with given args
template <typename... Args> void launch_assign_thread(std::thread& thread, Args... args) {
    // Join if possible to avoid std::terminate
    if(thread.joinable()) {
        thread.join();
    }
    // Launch thread
    thread = std::thread(args...);
}

// NOLINTBEGIN(performance-unnecessary-value-param)

State FSM::initialize(TransitionPayload payload) {
    auto call_wrapper = [this](const Configuration& config) {
        LOG(logger_, INFO) << "Calling initializing function of satellite...";
        const auto transition =
            call_satellite_function(this->satellite_.get(), &Satellite::initializing, Transition::initialized, config);
        this->reactIfAllowed(transition);
    };
    const auto config = std::get<Configuration>(std::move(payload));
    launch_assign_thread(transitional_thread_, call_wrapper, config);
    return State::initializing;
}

State FSM::launch(TransitionPayload /* payload */) {
    auto call_wrapper = [this]() {
        LOG(logger_, INFO) << "Calling launching function of satellite...";
        const auto transition = call_satellite_function(this->satellite_.get(), &Satellite::launching, Transition::launched);
        this->reactIfAllowed(transition);
    };
    launch_assign_thread(transitional_thread_, call_wrapper);
    return State::launching;
}

State FSM::land(TransitionPayload /* payload */) {
    auto call_wrapper = [this]() {
        LOG(logger_, INFO) << "Calling landing function of satellite...";
        const auto transition = call_satellite_function(this->satellite_.get(), &Satellite::landing, Transition::landed);
        this->reactIfAllowed(transition);
    };
    launch_assign_thread(transitional_thread_, call_wrapper);
    return State::landing;
}

State FSM::reconfigure(TransitionPayload payload) {
    auto call_wrapper = [this](const Configuration& partial_config) {
        LOG(logger_, INFO) << "Calling reconfiguring function of satellite...";
        const auto transition = call_satellite_function(
            this->satellite_.get(), &Satellite::reconfiguring, Transition::reconfigured, partial_config);
        this->reactIfAllowed(transition);
    };
    const auto partial_config = std::get<Configuration>(std::move(payload));
    launch_assign_thread(transitional_thread_, call_wrapper, partial_config);
    return State::reconfiguring;
}

State FSM::start(TransitionPayload payload) {
    auto call_wrapper = [this](std::uint32_t run_nr) {
        LOG(logger_, INFO) << "Calling starting function of satellite...";
        const auto transition =
            call_satellite_function(this->satellite_.get(), &Satellite::starting, Transition::started, run_nr);
        this->reactIfAllowed(transition);
    };
    const auto run_nr = std::get<std::uint32_t>(std::move(payload));
    launch_assign_thread(transitional_thread_, call_wrapper, run_nr);
    return State::starting;
}

State FSM::started(TransitionPayload /* payload */) {
    // Start running thread async
    auto call_wrapper = [this](const std::stop_token& stop_token) { satellite_->running(stop_token); };
    run_thread_ = std::jthread(call_wrapper);
    return State::RUN;
}

State FSM::stop(TransitionPayload /* payload */) {
    auto call_wrapper = [this]() {
        // First stop of RUN thread
        this->run_thread_.request_stop();
        if(this->run_thread_.joinable()) {
            this->run_thread_.join();
        }

        LOG(logger_, INFO) << "Calling stopping function of satellite...";
        const auto transition = call_satellite_function(this->satellite_.get(), &Satellite::stopping, Transition::stopped);
        this->reactIfAllowed(transition);
    };
    launch_assign_thread(transitional_thread_, call_wrapper);
    return State::stopping;
}

State FSM::interrupt(TransitionPayload /* payload */) {
    auto call_wrapper = [this](State previous_state) {
        LOG(logger_, INFO) << "Calling interrupting function of satellite...";
        const auto transition = call_satellite_function(
            this->satellite_.get(), &Satellite::interrupting, Transition::interrupted, previous_state);
        this->reactIfAllowed(transition);
    };
    launch_assign_thread(transitional_thread_, call_wrapper, this->state_);
    return State::interrupting;
}

State FSM::failure(TransitionPayload /* payload */) {
    auto call_wrapper = [this](State previous_state) {
        LOG(logger_, INFO) << "Calling on_failure function of satellite...";
        call_satellite_function(this->satellite_.get(), &Satellite::on_failure, Transition::failure, previous_state);
        // Note: we do not trigger a success transition as we always go to ERROR state
    };
    launch_assign_thread(failure_thread_, call_wrapper, this->state_);
    return State::ERROR;
}

// NOLINTEND(performance-unnecessary-value-param)
