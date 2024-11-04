/**
 * @file
 * @brief Implementation of a data transmitting satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "TransmitterSatellite.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <zmq.hpp>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/utils/networking.hpp"
#include "constellation/satellite/exceptions.hpp"

#include "Satellite.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::satellite;
using namespace constellation::utils;

TransmitterSatellite::TransmitterSatellite(std::string_view type, std::string_view name)
    : Satellite(type, name), cdtp_push_socket_(*global_zmq_context(), zmq::socket_type::push),
      cdtp_port_(bind_ephemeral_port(cdtp_push_socket_)), cdtp_logger_("CDTP") {
    // Only send to completed connections
    cdtp_push_socket_.set(zmq::sockopt::immediate, true);
    // Announce service via CHIRP
    auto* chirp_manager = chirp::Manager::getDefaultInstance();
    if(chirp_manager != nullptr) {
        chirp_manager->registerService(chirp::DATA, cdtp_port_);
    }
    LOG(cdtp_logger_, INFO) << "Data will be sent on port " << cdtp_port_;
}

void TransmitterSatellite::set_send_timeout(std::chrono::milliseconds timeout) {
    cdtp_push_socket_.set(zmq::sockopt::sndtimeo, static_cast<int>(timeout.count()));
}

TransmitterSatellite::DataMessage TransmitterSatellite::newDataMessage(std::size_t frames) {
    // Increase sequence counter and return new message
    return {getCanonicalName(), ++seq_, frames};
}

bool TransmitterSatellite::trySendDataMessage(TransmitterSatellite::DataMessage& message) {
    // Send data but do not wait for receiver
    LOG(cdtp_logger_, TRACE) << "Sending data message " << message.getHeader().getSequenceNumber();
    const auto sent = message.assemble().send(cdtp_push_socket_, static_cast<int>(zmq::send_flags::dontwait));
    if(!sent) [[unlikely]] {
        LOG(cdtp_logger_, DEBUG) << "Could not send message " << message.getHeader().getSequenceNumber();
    }

    return sent;
}

void TransmitterSatellite::sendDataMessage(TransmitterSatellite::DataMessage& message) {
    LOG(cdtp_logger_, TRACE) << "Sending data message " << message.getHeader().getSequenceNumber();
    const auto sent = message.assemble().send(cdtp_push_socket_);
    if(!sent) {
        throw SendTimeoutError("data message", data_msg_timeout_);
    }
}

void TransmitterSatellite::initializing_transmitter(Configuration& config) {
    data_bor_timeout_ = std::chrono::seconds(config.get<std::uint64_t>("_bor_timeout", 10));
    data_eor_timeout_ = std::chrono::seconds(config.get<std::uint64_t>("_eor_timeout", 10));
    data_msg_timeout_ = std::chrono::seconds(config.get<std::uint64_t>("_data_timeout", 10));
    LOG(cdtp_logger_, DEBUG) << "Timeout for BOR message " << data_bor_timeout_ << ", for EOR message " << data_eor_timeout_
                             << ", for DATA message " << data_msg_timeout_;
}

void TransmitterSatellite::reconfiguring_transmitter(const Configuration& partial_config) {
    if(partial_config.has("_bor_timeout")) {
        data_bor_timeout_ = std::chrono::seconds(partial_config.get<std::uint64_t>("_bor_timeout"));
        LOG(cdtp_logger_, DEBUG) << "Reconfigured timeout for BOR message: " << data_bor_timeout_;
    }
    if(partial_config.has("_eor_timeout")) {
        data_eor_timeout_ = std::chrono::seconds(partial_config.get<std::uint64_t>("_eor_timeout"));
        LOG(cdtp_logger_, DEBUG) << "Reconfigured timeout for EOR message: " << data_eor_timeout_;
    }
    if(partial_config.has("_data_timeout")) {
        data_msg_timeout_ = std::chrono::seconds(partial_config.get<std::uint64_t>("_data_timeout"));
        LOG(cdtp_logger_, DEBUG) << "Reconfigured timeout for DATA message: " << data_msg_timeout_;
    }
}

void TransmitterSatellite::starting_transmitter(std::string_view run_identifier, const config::Configuration& config) {
    // Reset run metadata and sequence counter
    seq_ = 0;
    run_metadata_ = {};
    set_run_metadata_tag("run_id", run_identifier);

    // Create CDTP1 message for BOR
    CDTP1Message msg {{getCanonicalName(), seq_, CDTP1Message::Type::BOR, std::chrono::system_clock::now(), bor_tags_}, 1};
    msg.addPayload(config.getDictionary().assemble());

    // Send BOR
    LOG(cdtp_logger_, DEBUG) << "Sending BOR message (timeout " << data_bor_timeout_ << ")";
    // Note: this is not interruptible, thus we set a send timeout to hang if no data receiver
    set_send_timeout(data_bor_timeout_);
    const auto sent = msg.assemble().send(cdtp_push_socket_);
    if(!sent) {
        throw SendTimeoutError("BOR message", data_bor_timeout_);
    }
    LOG(cdtp_logger_, DEBUG) << "Sent BOR message";

    // Set timeout for data sending
    set_send_timeout(data_msg_timeout_);

    // Clear BOR tags:
    bor_tags_ = {};
}

void TransmitterSatellite::stopping_transmitter() {
    // Create CDTP1 message for EOR
    CDTP1Message msg {{getCanonicalName(), ++seq_, CDTP1Message::Type::EOR, std::chrono::system_clock::now(), eor_tags_}, 1};
    msg.addPayload(run_metadata_.assemble());

    // Send EOR
    LOG(cdtp_logger_, DEBUG) << "Sending EOR message (" << data_eor_timeout_ << ")";
    // Note: this is not interruptible, thus we set a send timeout to prevent hang if no data receiver
    set_send_timeout(data_eor_timeout_);
    const auto sent = msg.assemble().send(cdtp_push_socket_);
    if(!sent) {
        throw SendTimeoutError("EOR message", data_eor_timeout_);
    }
    LOG(cdtp_logger_, DEBUG) << "Sent EOR message";

    // Clear EOR tags:
    eor_tags_ = {};
}
