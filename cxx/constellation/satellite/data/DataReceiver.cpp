/**
 * @file
 * @brief Implementation of data receiver for satellites
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "DataReceiver.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>

#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/logging/log.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/data/exceptions.hpp"

using namespace constellation;
using namespace constellation::config;
using namespace constellation::data;
using namespace constellation::message;
using namespace constellation::utils;

DataReceiver::DataReceiver()
    : socket_(context_, zmq::socket_type::pull), logger_("DATA_RECEIVER"), state_(State::BEFORE_BOR) {}

void DataReceiver::set_recv_timeout(std::chrono::milliseconds timeout) {
    socket_.set(zmq::sockopt::rcvtimeo, static_cast<int>(timeout.count()));
}

void DataReceiver::initializing(Configuration& config) {
    // Get canonical name of sender to receive data from
    sender_name_ = config.get<std::string>("_data_sender_name");
    LOG(logger_, DEBUG) << "Initialized data receiver for satellite " << sender_name_;
    // TODO: set logger topic to DATA_RECEIVER_<sender_name>, maybe create late?

    data_bor_timeout_ = std::chrono::seconds(config.get<std::uint64_t>("_data_bor_timeout", 10));
    data_data_timeout_ = std::chrono::seconds(config.get<std::uint64_t>("_data_data_timeout", 1));
    data_eor_timeout_ = std::chrono::seconds(config.get<std::uint64_t>("_data_eor_timeout", 10));
    LOG(logger_, DEBUG) << "Timeout for BOR message " << data_bor_timeout_ << ", for DATA messages " << data_data_timeout_
                        << ", for EOR message " << data_eor_timeout_;

    // Send request for DATA services early
    chirp::Manager::getDefaultInstance()->sendRequest(chirp::DATA);

    state_ = State::BEFORE_BOR;
}

Dictionary DataReceiver::starting() {
    // Send request for DATA services
    chirp::Manager::getDefaultInstance()->sendRequest(chirp::DATA);

    // Find via CHIRP
    uri_ = {};
    auto* chirp_manager = chirp::Manager::getDefaultInstance();
    while(uri_.empty()) { // TODO add timeout
        for(const auto& service : chirp_manager->getDiscoveredServices(chirp::DATA)) {
            if(service.host_id == message::MD5Hash(sender_name_)) {
                uri_ = service.to_uri();
                break;
            }
        }
        // Wait a bit to give satellite time to announce itself
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    LOG(logger_, DEBUG) << "Connecting to " << uri_.c_str();

    // Recreate socket (since it might be closed) and connect
    socket_ = zmq::socket_t(context_, zmq::socket_type::pull);
    socket_.connect(uri_.c_str());

    // Check that before BOR
    if(state_ != State::BEFORE_BOR) [[unlikely]] {
        throw InvalidDataState("starting", to_string(state_));
    }

    // Receive BOR
    LOG(logger_, DEBUG) << "Receiving BOR message (timeout " << data_bor_timeout_ << ")";
    // Note: this is not interruptible, thus we set a recv timeout to prevent hang if no data receiver
    set_recv_timeout(data_bor_timeout_);
    zmq::multipart_t msgs {};
    const auto received = msgs.recv(socket_);
    if(!received) {
        throw RecvTimeoutError("BOR message", data_bor_timeout_);
    }

    // Reset timeout for data receiving
    set_recv_timeout(data_data_timeout_);

    // Decode
    auto bor_msg = CDTP1Message::disassemble(msgs);
    if(bor_msg.getHeader().getType() != CDTP1Message::Type::BOR) [[unlikely]] {
        throw InvalidMessageType(bor_msg.getHeader().getType(), CDTP1Message::Type::BOR);
    }
    auto config = Dictionary::disassemble(bor_msg.getPayload().at(0));

    // Reset sequence counter
    seq_ = bor_msg.getHeader().getSequenceNumber();

    // Set state to allow for recvData
    state_ = State::IN_RUN;

    return config;
}

std::optional<CDTP1Message> DataReceiver::recvData() {
    // If EOR already received, return immediately
    if(state_ == State::GOT_EOR) [[unlikely]] {
        return std::nullopt;
    }
    // Otherwise check that in RUN or stopping
    else if(state_ != State::IN_RUN && state_ != State::STOPPING) [[unlikely]] {
        throw InvalidDataState("recvData", to_string(state_));
    }

    // Receive CDTP1 message
    zmq::multipart_t msgs {};
    LOG(logger_, TRACE) << "Trying to receive data message " << seq_ + 1;
    const auto received = msgs.recv(socket_);
    if(!received) {
        // If we are stopping, no message means timeout is reached
        if(state_ == State::STOPPING) [[unlikely]] {
            throw RecvTimeoutError("EOR", data_eor_timeout_);
        }
        return std::nullopt;
    }
    // Increment received message counter:
    ++seq_;
    LOG(logger_, TRACE) << "Received data message " << seq_;

    // Decode
    auto msg = CDTP1Message::disassemble(msgs);
    const auto msg_type = msg.getHeader().getType();
    if(msg_type == CDTP1Message::Type::EOR) [[unlikely]] {
        LOG(logger_, DEBUG) << "Received EOR message";
        eor_ = Dictionary::disassemble(msg.getPayload().at(0));
        state_ = State::GOT_EOR;
        return std::nullopt;
    } else if(msg_type != CDTP1Message::Type::DATA) [[unlikely]] {
        throw InvalidMessageType(msg_type, CDTP1Message::Type::DATA);
    }

    const auto seq = msg.getHeader().getSequenceNumber();
    if(seq != seq_) [[unlikely]] {
        LOG(logger_, WARNING) << "Discrepancy in data message sequence: counted " << seq_ << ", received " << seq;
        seq_ = seq;
    }

    return msg;
}

void DataReceiver::stopping() {
    // Check that in RUN or already EOR received
    if(state_ != State::IN_RUN && state_ != State::GOT_EOR) [[unlikely]] {
        throw InvalidDataState("stopping", to_string(state_));
    }

    // Set to (longer) EOR timeout for last data messages and EOR
    set_recv_timeout(data_eor_timeout_);

    // If EOR not received yet, set state to tell recvData to throw if no message received
    if(state_ != State::GOT_EOR) {
        state_ = State::STOPPING;
    }
}

bool DataReceiver::gotEOR() {
    return state_ == State::GOT_EOR;
}

const Dictionary& DataReceiver::getEOR() {
    // Check that EOR already received
    if(state_ != State::GOT_EOR) [[unlikely]] {
        throw InvalidDataState("getEOR", to_string(state_));
    }

    // Set state to allow for beginRun again
    state_ = State::BEFORE_BOR;

    // Close socket to drop any remaining messages
    socket_.disconnect(uri_.c_str());
    socket_.close();

    return eor_;
}
