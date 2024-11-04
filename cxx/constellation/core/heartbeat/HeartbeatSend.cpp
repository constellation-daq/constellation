/**
 * @file
 * @brief Implementation of Heartbeat sender
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "HeartbeatSend.hpp"

#include <chrono>
#include <functional>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

#include <zmq.hpp>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/message/CHP1Message.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/networking.hpp"

using namespace constellation;
using namespace constellation::heartbeat;
using namespace constellation::message;
using namespace constellation::protocol;
using namespace constellation::utils;

HeartbeatSend::HeartbeatSend(std::string sender,
                             std::function<CSCP::State()> state_callback,
                             std::chrono::milliseconds interval)
    : pub_socket_(*global_zmq_context(), zmq::socket_type::pub), port_(bind_ephemeral_port(pub_socket_)),
      sender_(std::move(sender)), state_callback_(std::move(state_callback)), interval_(interval) {

    // Announce service via CHIRP
    auto* chirp_manager = chirp::Manager::getDefaultInstance();
    if(chirp_manager != nullptr) {
        chirp_manager->registerService(chirp::HEARTBEAT, port_);
    }

    sender_thread_ = std::jthread(std::bind_front(&HeartbeatSend::loop, this));
}

HeartbeatSend::~HeartbeatSend() {

    // Send CHIRP depart message
    auto* chirp_manager = chirp::Manager::getDefaultInstance();
    if(chirp_manager != nullptr) {
        chirp_manager->unregisterService(chirp::HEARTBEAT, port_);
    }

    // Stop sender thread
    sender_thread_.request_stop();
    if(sender_thread_.joinable()) {
        sender_thread_.join();
    }
}

void HeartbeatSend::sendExtrasystole() {
    cv_.notify_one();
}

void HeartbeatSend::loop(const std::stop_token& stop_token) {
    // Notify condition variable when stop is requested
    const std::stop_callback stop_callback {stop_token, [&]() { cv_.notify_all(); }};

    while(!stop_token.stop_requested()) {
        std::unique_lock<std::mutex> lock {mutex_};
        // Wait until condition variable is notified or timeout is reached
        cv_.wait_for(lock, interval_.load() / 2);

        // Publish CHP message with current state
        CHP1Message(sender_, state_callback_(), interval_.load()).assemble().send(pub_socket_);
    }
}
