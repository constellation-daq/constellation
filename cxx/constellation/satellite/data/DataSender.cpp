/**
 * @file
 * @brief Implementation of data sender for satellites
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "DataSender.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include <zmq.hpp>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/utils/ports.hpp"
#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/data/exceptions.hpp"

using namespace constellation;
using namespace constellation::config;
using namespace constellation::data;
using namespace constellation::message;
using namespace constellation::utils;

DataSender::DataSender(std::string sender_name)
    : socket_(context_, zmq::socket_type::push), port_(bind_ephemeral_port(socket_)), sender_name_(std::move(sender_name)),
      logger_("DATA_SENDER"), state_(State::BEFORE_BOR) {
    // Only send to completed connections
    socket_.set(zmq::sockopt::immediate, true);
    // Announce service via CHIRP
    auto* chirp_manager = chirp::Manager::getDefaultInstance();
    if(chirp_manager != nullptr) {
        chirp_manager->registerService(chirp::DATA, port_);
    } else {
        LOG(logger_, WARNING) << "Failed to advertise data sender on the network, satellite might not be discovered";
    }
    LOG(logger_, INFO) << "Data will be sent on port " << port_;
}

void DataSender::set_send_timeout(std::chrono::milliseconds timeout) {
    socket_.set(zmq::sockopt::sndtimeo, static_cast<int>(timeout.count()));
}

void DataSender::initializing(Configuration& config) {
    data_bor_timeout_ = std::chrono::seconds(config.get<std::uint64_t>("_data_bor_timeout", 10));
    data_eor_timeout_ = std::chrono::seconds(config.get<std::uint64_t>("_data_eor_timeout", 10));
    LOG(logger_, DEBUG) << "Timeout for BOR message " << data_bor_timeout_ << ", for EOR message " << data_eor_timeout_;

    state_ = State::BEFORE_BOR;
}

void DataSender::reconfiguring(const config::Configuration& partial_config) {
    if(partial_config.has("_data_bor_timeout")) {
        data_bor_timeout_ = std::chrono::seconds(partial_config.get<std::uint64_t>("_data_chirp_timeout"));
        LOG(logger_, DEBUG) << "Reconfigured timeout for BOR message: " << data_bor_timeout_;
    }
    if(partial_config.has("_data_eor_timeout")) {
        data_eor_timeout_ = std::chrono::seconds(partial_config.get<std::uint64_t>("_data_eor_timeout"));
        LOG(logger_, DEBUG) << "Reconfigured timeout for EOR message: " << data_eor_timeout_;
    }
}

void DataSender::starting(const Configuration& config) {
    // Check that before BOR
    if(state_ != State::BEFORE_BOR) [[unlikely]] {
        throw InvalidDataState("starting", to_string(state_));
    }

    // Reset run metadata and sequence counter
    seq_ = 0;
    run_metadata_ = {};

    // Create CDTP1 message for BOR
    CDTP1Message msg {{sender_name_, 0, CDTP1Message::Type::BOR}, 1};
    msg.addPayload(config.getDictionary(Configuration::Group::ALL, Configuration::Usage::USED).assemble());

    // Send BOR
    LOG(logger_, DEBUG) << "Sending BOR message (timeout " << data_bor_timeout_ << ")";
    // Note: this is not interruptible, thus we set a send timeout to hang if no data receiver
    set_send_timeout(data_bor_timeout_);
    const auto sent = msg.assemble().send(socket_);
    if(!sent) {
        throw SendTimeoutError("BOR message", data_eor_timeout_);
    }

    // Reset timeout for data sending
    set_send_timeout();

    // Set state to allow for sendData
    state_ = State::IN_RUN;
}

DataSender::DataMessage DataSender::newDataMessage(std::size_t frames) {
    // Check that in RUN
    if(state_ != State::IN_RUN) [[unlikely]] {
        throw InvalidDataState("newDataMessage", to_string(state_));
    }

    // Increase sequence counter for new message
    ++seq_;

    // Return new message
    return {sender_name_, seq_, frames};
}

bool DataSender::sendDataMessage(DataSender::DataMessage& message) {
    // Check that in RUN
    if(state_ != State::IN_RUN) [[unlikely]] {
        throw InvalidDataState("sendDataMessage", to_string(state_));
    }

    // Send data but do not wait for receiver
    LOG(logger_, TRACE) << "Sending data message " << message.getHeader().getSequenceNumber();
    const auto success = message.assemble().send(socket_, static_cast<int>(zmq::send_flags::dontwait));

    if(!success) {
        LOG(logger_, DEBUG) << "Could not send message " << message.getHeader().getSequenceNumber();
    }

    return success;
}

void DataSender::setRunMetadata(Dictionary run_metadata) {
    run_metadata_ = std::move(run_metadata);
}

void DataSender::stopping() {
    // Check that in RUN
    if(state_ != State::IN_RUN) [[unlikely]] {
        throw InvalidDataState("stopping", to_string(state_));
    }

    // Create CDTP1 message for EOR
    CDTP1Message msg {{sender_name_, ++seq_, CDTP1Message::Type::EOR}, 1};
    msg.addPayload(run_metadata_.assemble());

    // Send EOR
    LOG(logger_, DEBUG) << "Sending EOR message (" << data_eor_timeout_ << ")";
    // Note: this is not interruptible, thus we set a send timeout to prevent hang if no data receiver
    set_send_timeout(data_eor_timeout_);
    const auto sent = msg.assemble().send(socket_);
    if(!sent) {
        throw SendTimeoutError("EOR message", data_eor_timeout_);
    }

    // Set state to allow for beginRun again
    state_ = State::BEFORE_BOR;
}
