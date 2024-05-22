/**
 * @file
 * @brief FSM enums
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <utility>

#include "constellation/core/message/satellite_definitions.hpp"
#include "constellation/core/utils/std_future.hpp"

namespace constellation::satellite {

    // Forward possible FSM states
    using message::State;

    // Forward possible FSM transitions
    using message::Transition;

    // Forward possible FSM transition commands
    using message::TransitionCommand;

    inline constexpr bool is_steady(State state) {
        // Lower four bytes are 0
        return (static_cast<unsigned int>(std::to_underlying(state)) & 0x0FU) == 0x00U;
    }

    inline constexpr bool is_shutdown_allowed(State state) {
        // Regular shutdown only allowed from states NEW, INIT, SAFE and ERROR:
        return (state == State::NEW || state == State::INIT || state == State::SAFE || state == State::ERROR);
    }

} // namespace constellation::satellite
