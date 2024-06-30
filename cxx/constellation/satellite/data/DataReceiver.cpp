/**
 * @file
 * @brief Implementation of data receiver for arbitrary endpoints
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "DataReceiver.hpp"

#include <cstdint>
#include <iomanip>
#include <string>

#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/core/utils/timers.hpp"
#include "constellation/satellite/data/exceptions.hpp"
#include "constellation/satellite/exceptions.hpp"

using namespace constellation;
using namespace constellation::config;
using namespace constellation::data;
using namespace constellation::message;
using namespace constellation::satellite;
using namespace constellation::utils;

DataRecv::DataRecv()
    : BasePool<message::CDTP1Message>(
          chirp::DATA, logger_, std::bind_front(&DataRecv::receive_impl, this), zmq::socket_type::pull),
      logger_("DATA") {}

void DataRecv::socket_connected(zmq::socket_t&) {
    LOG(logger_, STATUS) << "New datasender connected";
}

void DataRecv::receive_impl(const message::CDTP1Message& msg) {

    const auto msg_type = msg.getHeader().getType();
    const auto msg_seq = msg.getHeader().getSequenceNumber();
    const auto msg_sender = msg.getHeader().getSender();

    LOG(logger_, TRACE) << "Received message: " << msg_sender << " " << to_string(msg_type) << " " << msg_seq;

    auto states_it = states_.find(msg_sender);
    if(states_it == states_.end()) {
        LOG(logger_, DEBUG) << "First message from new sender " << std::quoted(msg_sender);
        const auto [it, inserted] = states_.emplace(to_string(msg_sender), Sender {State::AWAITING_BOR, 0});
        states_it = it;
    }

    if(states_it->second.state == State::AWAITING_BOR) {
        if(msg_type != CDTP1Message::Type::BOR) {
            // throw InvalidMessageType(msg_type, CDTP1Message::Type::BOR);
            LOG(logger_, CRITICAL) << "NOT BOR!";
        }

        // This is BOR, change state and update sequence:
        LOG(logger_, DEBUG) << "Received BOR message from " << std::quoted(msg_sender);
        states_it->second.seq = msg_seq;
        states_it->second.state = State::AWAITING_DATA;
    } else if(states_it->second.state == State::AWAITING_DATA) {
        if(msg_type == CDTP1Message::Type::EOR) [[unlikely]] {
            LOG(logger_, DEBUG) << "Received EOR message from " << std::quoted(msg_sender);
            // Reset to pre-run state, waiting for BOR:
            states_it->second.state = State::AWAITING_BOR;
        } else if(msg_type != CDTP1Message::Type::DATA) [[unlikely]] {
            LOG(logger_, CRITICAL) << "NOT DATA!";
            // throw InvalidMessageType(msg_type, CDTP1Message::Type::DATA);
        }
    }

    // Check correct sequence:
    if(msg_seq != states_it->second.seq) [[unlikely]] {
        LOG(logger_, WARNING) << "Discrepancy in data message sequence: counted " << states_it->second.seq << ", received "
                              << msg_seq;
        states_it->second.seq = msg_seq;
    }

    // Pass message to data handler for treatment
    receive(msg);

    // Increment received message counter for next message
    ++states_it->second.seq;
}
