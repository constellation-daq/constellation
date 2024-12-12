/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <string>
#include <thread>
#include <tuple>
#include <utility>

#include "constellation/controller/Controller.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"

class DummyController : public constellation::controller::Controller {
public:
    using Controller::UpdateType;

    DummyController(std::string controller_name) : Controller(std::move(controller_name)) {}

    void reached_state(constellation::protocol::CSCP::State state, bool global) final {
        reached_state_global_ = global;
        reached_state_ = state;
        reached_ = true;
    }

    void propagate_update(constellation::controller::Controller::UpdateType type,
                          std::size_t position,
                          std::size_t total) final {
        propagate_update_ = type;
        propagate_position_ = position;
        propagate_total_ = total;
        propagate_ = true;
    }

    std::tuple<UpdateType, std::size_t, std::size_t> lastPropagateUpdate() const {
        return {propagate_update_.load(), propagate_position_.load(), propagate_total_.load()};
    };

    void waitReachedState(constellation::protocol::CSCP::State state, bool global) {
        using namespace std::chrono_literals;

        // Wait for callback to trigger
        while(!reached_.load() || reached_state_.load() != state || reached_state_global_.load() != global) {
            std::this_thread::sleep_for(50ms);
        }
        reached_.store(false);
    }

    void waitPropagateUpdate() {
        using namespace std::chrono_literals;

        // Wait for callback to trigger
        while(!propagate_.load()) {
            std::this_thread::sleep_for(50ms);
        }
        propagate_.store(false);
    }

private:
    std::atomic_bool reached_ {false};
    std::atomic_bool reached_state_global_ {true};
    std::atomic<constellation::protocol::CSCP::State> reached_state_ {constellation::protocol::CSCP::State::NEW};

    std::atomic_bool propagate_ {false};
    std::atomic_size_t propagate_total_ {0};
    std::atomic_size_t propagate_position_ {0};
    std::atomic<constellation::controller::Controller::UpdateType> propagate_update_;
};
