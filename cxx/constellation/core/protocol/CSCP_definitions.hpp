/**
 * @file
 * @brief Additional definitions for the CSCP protocol
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdint>
#include <regex>
#include <string>
#include <type_traits>
#include <utility>

#include <magic_enum.hpp>

#include "constellation/core/utils/std_future.hpp"

namespace constellation::protocol::CSCP {

    /** Possible Satellite FSM states */
    enum class State : std::uint8_t {
        NEW = 0x10,
        initializing = 0x12,
        INIT = 0x20,
        launching = 0x23,
        ORBIT = 0x30,
        landing = 0x32,
        reconfiguring = 0x33,
        starting = 0x34,
        RUN = 0x40,
        stopping = 0x43,
        interrupting = 0x0E,
        SAFE = 0xE0,
        ERROR = 0xF0,
    };

    /** Possible FSM transitions */
    enum class Transition : std::uint8_t {
        initialize,
        initialized,
        launch,
        launched,
        land,
        landed,
        reconfigure,
        reconfigured,
        start,
        started,
        stop,
        stopped,
        interrupt,
        interrupted,
        failure,
    };

    /** Possible transition commands via CSCP */
    enum class TransitionCommand : std::underlying_type_t<Transition> {
        initialize = std::to_underlying(Transition::initialize),
        launch = std::to_underlying(Transition::launch),
        land = std::to_underlying(Transition::land),
        reconfigure = std::to_underlying(Transition::reconfigure),
        start = std::to_underlying(Transition::start),
        stop = std::to_underlying(Transition::stop),
    };

    /** Possible standard (non-transition) commands via CSCP */
    enum class StandardCommand : std::underlying_type_t<TransitionCommand> {
        get_name,
        get_version,
        get_commands,
        get_state,
        get_status,
        get_config,
        get_run_id,
        shutdown,
    };

    /**
     * @brief Check if a state is steady
     */
    constexpr bool is_steady(State state) {
        // In steady states the lower four bytes are 0
        return (static_cast<unsigned int>(std::to_underlying(state)) & 0x0FU) == 0x00U;
    }

    /**
     * @brief Check if the CSCP shutdown command is allowed from a given state
     *
     * Shutdown is only allowed from NEW, INIT, SAFE and ERROR.
     */
    constexpr bool is_shutdown_allowed(State state) {
        using enum State;
        return (state == NEW || state == INIT || state == SAFE || state == ERROR);
    }

    /**
     * @brief Check if given state is in one of template states list
     *
     * @param state State to check
     * @return True if `state` equals one of the states given in the template parameters
     */
    template <State... states> constexpr bool is_one_of_states(State state) {
        return ((state == states) || ...);
    }

    /**
     * @brief Check if given state is not in one of template states list
     *
     * @param state State to check
     * @return True if `state` equals none of the states given in the template parameters
     */
    template <State... states> static constexpr bool is_not_one_of_states(State state) {
        return ((state != states) && ...);
    }

    /**
     * @brief Checks if a satellite name is valid
     *
     * A satellite name may contain alphanumeric characters and underscores and may not be empty.
     */
    inline bool is_valid_satellite_name(const std::string& satellite_name) {
        return std::regex_match(satellite_name, std::regex("^\\w+$"));
    }

    /**
     * @brief Checks if a run ID is valid
     *
     * A run ID may contain alphanumeric characters, underscores or dashes and may not be empty.
     */
    inline bool is_valid_run_id(const std::string& run_id) {
        return std::regex_match(run_id, std::regex("^[\\w-]+$"));
    }

    /**
     * @brief Checks if a command name is valid
     *
     * A command may contain alphanumeric characters or underscores, and may not be empty or start with a digit.
     */
    inline bool is_valid_command_name(const std::string& command_name) {
        return std::regex_match(command_name, std::regex("^\\D\\w*$"));
    }

} // namespace constellation::protocol::CSCP

// State enum exceeds default enum value limits of magic_enum (-128, 127)
namespace magic_enum::customize {
    template <> struct enum_range<constellation::protocol::CSCP::State> {
        static constexpr int min = 0;
        static constexpr int max = 255;
    };
} // namespace magic_enum::customize
