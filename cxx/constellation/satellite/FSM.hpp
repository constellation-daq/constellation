/**
 * @file
 * @brief FSM class
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include "constellation/build.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"

namespace constellation::satellite {
    class BaseSatellite;

    class FSM {
    public:
        using State = protocol::CSCP::State;
        using Transition = protocol::CSCP::Transition;
        using TransitionCommand = protocol::CSCP::TransitionCommand;

        /** Payload of a transition function: variant with config, partial_config or run identifier */
        using TransitionPayload = std::variant<std::monostate, config::Configuration, std::string>;

        /** Function pointer for a transition function: takes the variant mentioned above, returns new State */
        using TransitionFunction = State (FSM::*)(TransitionPayload);

        /** Maps the allowed transitions of a state to a transition function */
        using TransitionMap = std::map<Transition, TransitionFunction>;

        /** Maps state to transition maps for that state */
        using StateTransitionMap = std::map<State, TransitionMap>;

    public:
        /**
         * @brief Construct the final state machine of a satellite
         *
         * @param satellite Satellite class with functions of transitional states
         */
        FSM(BaseSatellite* satellite) : satellite_(satellite), logger_("FSM") {}

        CNSTLN_API ~FSM();

        // No copy/move constructor/assignment
        /// @cond doxygen_suppress
        FSM(const FSM& other) = delete;
        FSM& operator=(const FSM& other) = delete;
        FSM(FSM&& other) = delete;
        FSM& operator=(FSM&& other) = delete;
        /// @endcond

        /**
         * @brief Returns the current state of the FSM
         */
        State getState() const { return state_.load(); }

        /**
         * @brief Check if a FSM transition is allowed in current state
         *
         * @param transition Transition to check if allowed
         * @return True if transition is possible
         */
        CNSTLN_API bool isAllowed(Transition transition) const;

        /**
         * @brief Perform a FSM transition
         *
         * @param transition Transition to perform
         * @param payload Payload for the transition function
         * @throw FSMError if the transition is not a valid transition in the current state
         */
        CNSTLN_API void react(Transition transition, TransitionPayload payload = {});

        /**
         * @brief Perform a FSM transition if allowed, otherwise do nothing
         *
         * @param transition Transition to perform if allowed
         * @param payload Payload for the transition function
         * @return True if the transition was initiated
         */
        CNSTLN_API bool reactIfAllowed(Transition transition, TransitionPayload payload = {});

        /**
         * @brief Perform a FSM transition via a CSCP message
         *
         * @param transition_command Transition command from CSCP
         * @param payload Payload frame from CSCP
         * @return Tuple containing the CSCP message type and a description
         */
        CNSTLN_API std::pair<message::CSCP1Message::Type, std::string> reactCommand(TransitionCommand transition_command,
                                                                                    const message::PayloadBuffer& payload);

        /**
         * @brief Try to perform an interrupt as soon as possible
         *
         * This function waits for the next steady state and performs an interrupt if in ORBIT or RUN state, otherwise
         * nothing is done. It guarantees that the FSM is in a state where the satellite can be safely shut down.
         *
         * @param reason Reason for the requested interrupt
         *
         * @warning This function is not thread safe, meaning that no other react command should be called during execution.
         */
        CNSTLN_API void requestInterrupt(std::string_view reason);

        /**
         * @brief Registering a callback to be executed when a new state was entered
         *
         * This function adds a new state update callback. Registered callbacks are used to distribute the state of the FSM
         * whenever it was changed
         *
         * @param callback Callback taking the new state as argument
         */
        CNSTLN_API void registerStateCallback(std::function<void(State)> callback);

    private:
        /**
         * @brief Find the transition function for a given transition in the current state
         *
         * @param transition Transition to search for a transition function
         * @return Transition function corresponding to the transition
         * @throw FSMError if the transition is not a valid transition in the current state
         */
        TransitionFunction find_transition_function(Transition transition) const;

        /**
         * @brief Call all state callbacks
         */
        void call_state_callbacks();

        /**
         * @brief Call a satellite function
         *
         * @param func Function to be called
         * @param success_transition Transition to be performed once the function successfully returned
         * @param args Function arguments
         * @return Transition after function call, either success transition or failure
         */
        template <typename Func, typename... Args>
        Transition call_satellite_function(Func func, Transition success_transition, Args&&... args);

        /**
         * @brief Stop and join the run_thread
         */
        void stop_run_thread();

        CNSTLN_API auto initialize(TransitionPayload payload) -> State;
        CNSTLN_API auto initialized(TransitionPayload payload) -> State;
        CNSTLN_API auto launch(TransitionPayload payload) -> State;
        CNSTLN_API auto launched(TransitionPayload payload) -> State;
        CNSTLN_API auto land(TransitionPayload payload) -> State;
        CNSTLN_API auto landed(TransitionPayload payload) -> State;
        CNSTLN_API auto reconfigure(TransitionPayload payload) -> State;
        CNSTLN_API auto reconfigured(TransitionPayload payload) -> State;
        CNSTLN_API auto start(TransitionPayload payload) -> State;
        CNSTLN_API auto started(TransitionPayload payload) -> State;
        CNSTLN_API auto stop(TransitionPayload payload) -> State;
        CNSTLN_API auto stopped(TransitionPayload payload) -> State;
        CNSTLN_API auto interrupt(TransitionPayload payload) -> State;
        CNSTLN_API auto interrupted(TransitionPayload payload) -> State;
        CNSTLN_API auto failure(TransitionPayload payload) -> State;

        // clang-format off
        const StateTransitionMap state_transition_map_ { // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
            {State::NEW, {
                {Transition::initialize, &FSM::initialize},
                {Transition::failure, &FSM::failure},
            }},
            {State::initializing, {
                {Transition::initialized, &FSM::initialized},
                {Transition::failure, &FSM::failure},
            }},
            {State::INIT, {
                {Transition::initialize, &FSM::initialize},
                {Transition::launch, &FSM::launch},
                {Transition::failure, &FSM::failure},
            }},
            {State::launching, {
                {Transition::launched, &FSM::launched},
                {Transition::failure, &FSM::failure},
            }},
            {State::landing, {
                {Transition::landed, &FSM::landed},
                {Transition::failure, &FSM::failure},
            }},
            {State::ORBIT, {
                {Transition::land, &FSM::land},
                {Transition::reconfigure, &FSM::reconfigure},
                {Transition::start, &FSM::start},
                {Transition::interrupt, &FSM::interrupt},
                {Transition::failure, &FSM::failure},
            }},
            {State::reconfiguring, {
                {Transition::reconfigured, &FSM::reconfigured},
                {Transition::failure, &FSM::failure},
            }},
            {State::starting, {
                {Transition::started, &FSM::started},
                {Transition::failure, &FSM::failure},
            }},
            {State::stopping, {
                {Transition::stopped, &FSM::stopped},
                {Transition::failure, &FSM::failure},
            }},
            {State::RUN, {
                {Transition::stop, &FSM::stop},
                {Transition::interrupt, &FSM::interrupt},
                {Transition::failure, &FSM::failure},
            }},
            {State::interrupting, {
                {Transition::interrupted, &FSM::interrupted},
                {Transition::failure, &FSM::failure},
            }},
            {State::SAFE, {
                {Transition::initialize, &FSM::initialize},
                {Transition::failure, &FSM::failure},
            }},
            {State::ERROR, {
                {Transition::initialize, &FSM::initialize},
            }},
        };
        // clang-format on

    private:
        std::atomic<State> state_ {State::NEW};
        BaseSatellite* satellite_;
        log::Logger logger_;
        std::mutex transition_mutex_;
        std::thread transitional_thread_;
        std::jthread run_thread_;
        std::thread failure_thread_;

        /** State update callback */
        std::vector<std::function<void(State)>> state_callbacks_;
        std::mutex state_callbacks_mutex_;
    };

} // namespace constellation::satellite
