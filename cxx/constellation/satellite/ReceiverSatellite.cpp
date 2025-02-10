/**
 * @file
 * @brief Implementation of a data receiving satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "ReceiverSatellite.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <ranges>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/config/Value.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/metrics/Metric.hpp"
#include "constellation/core/metrics/stat.hpp"
#include "constellation/core/networking/exceptions.hpp"
#include "constellation/core/pools/BasePool.hpp"
#include "constellation/core/protocol/CDTP_definitions.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/core/utils/timers.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation;
using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::metrics;
using namespace constellation::networking;
using namespace constellation::pools;
using namespace constellation::protocol;
using namespace constellation::satellite;
using namespace constellation::utils;
using namespace std::chrono_literals;

ReceiverSatellite::ReceiverSatellite(std::string_view type, std::string_view name)
    : Satellite(type, name),
      BasePool("CDTP", [this](CDTP1Message&& message) { this->handle_cdtp_message(std::move(message)); }),
      cdtp_logger_("CDTP") {

    register_timed_metric("BYTES_RECEIVED",
                          "B",
                          MetricType::LAST_VALUE,
                          "Number of bytes received by this satellite in the current run",
                          10s,
                          {CSCP::State::starting, CSCP::State::RUN, CSCP::State::stopping},
                          [this]() { return bytes_received_.load(); });
}

void ReceiverSatellite::running(const std::stop_token& stop_token) {
    while(!stop_token.stop_requested()) {
        // Check and rethrow exception from BasePool
        checkPoolException();

        // Wait a bit to avoid hot loop
        std::this_thread::sleep_for(100ms);
    }
}

bool ReceiverSatellite::should_connect(const chirp::DiscoveredService& service) {
    return std::ranges::any_of(data_transmitters_,
                               [=](const auto& data_tramsitter) { return service.host_id == MD5Hash(data_tramsitter); });
}

void ReceiverSatellite::initializing_receiver(Configuration& config) {
    data_eor_timeout_ = std::chrono::seconds(config.get<std::uint64_t>("_eor_timeout", 10));
    LOG(cdtp_logger_, DEBUG) << "Timeout for EOR message " << data_eor_timeout_;

    data_transmitters_ = config.getArray<std::string>("_data_transmitters");
    reset_data_transmitter_states();
    LOG(cdtp_logger_, INFO) << "Initialized to receive data from " << range_to_string(data_transmitters_);
}

void ReceiverSatellite::reconfiguring_receiver(const Configuration& partial_config) {
    if(partial_config.has("_eor_timeout")) {
        data_eor_timeout_ = std::chrono::seconds(partial_config.get<std::uint64_t>("_eor_timeout"));
        LOG(cdtp_logger_, DEBUG) << "Reconfigured timeout for EOR message: " << data_eor_timeout_;
    }

    if(partial_config.has("_data_transmitters")) {
        // BasePool disconnect all sockets when stopped, so this is safe to do
        data_transmitters_ = partial_config.getArray<std::string>("_data_transmitters");
        reset_data_transmitter_states();
        LOG(cdtp_logger_, INFO) << "Reconfigured to receive data from " << range_to_string(data_transmitters_);
    }
}

void ReceiverSatellite::starting_receiver() {
    // Reset all transmitters to not connected
    reset_data_transmitter_states();

    // Reset bytes received metric
    bytes_received_ = 0;
    STAT("BYTES_RECEIVED", 0);

    // Start BasePool thread
    startPool();
}

void ReceiverSatellite::stopping_receiver() {
    // Wait until no more events returned by poller
    while(pollerEvents() > 0) {
        // Check any exceptions that prevents poller from continuing
        checkPoolException();

        LOG(cdtp_logger_, TRACE) << "Poller still returned events, waiting before checking for EOR arrivals";

        // Wait a bit to avoid hot loop
        std::this_thread::sleep_for(100ms);
    }
    // Now start EOR timer
    LOG(cdtp_logger_, DEBUG) << "Starting timeout for EOR arrivals (" << data_eor_timeout_ << ")";
    TimeoutTimer timer {data_eor_timeout_};
    timer.reset();

    // Warn about transmitters that never sent a BOR message
    if(cdtp_logger_.shouldLog(WARNING)) {
        const std::lock_guard data_transmitter_states_lock {data_transmitter_states_mutex_};
        auto data_transmitters_not_connected =
            std::ranges::views::filter(data_transmitter_states_, [](const auto& data_transmitter_p) {
                return data_transmitter_p.second.state == TransmitterState::NOT_CONNECTED;
            });
        // Note: we do not have a biderectional range so the content needs to be copied to a vector
        LOG_IF(cdtp_logger_, WARNING, !data_transmitters_not_connected.empty())
            << "BOR message never send from "
            << range_to_string(std::ranges::to<std::vector>(std::views::keys(data_transmitters_not_connected)));

        // Warn about missed messages (so far)
        auto data_transmitters_missed_msg = std::ranges::views::filter(
            data_transmitter_states_, [](const auto& data_transmitter_p) { return data_transmitter_p.second.missed > 0; });
        LOG_IF(cdtp_logger_, WARNING, !data_transmitters_missed_msg.empty())
            << "Missed messages from "
            << range_to_string(std::ranges::to<std::vector>(std::views::keys(data_transmitters_missed_msg)))
            << ", data might be incomplete";
    }

    // Loop until all data transmitters that sent a BOR also sent an EOR
    while(true) {
        std::unique_lock data_transmitter_states_lock {data_transmitter_states_mutex_};
        const auto missing_eors = std::ranges::count_if(data_transmitter_states_, [](const auto& data_transmitter_p) {
            return data_transmitter_p.second.state == TransmitterState::BOR_RECEIVED;
        });
        data_transmitter_states_lock.unlock();

        // Check if EOR messages are missing
        if(missing_eors == 0) {
            break;
        }

        // If timeout reached, throw
        if(timer.timeoutReached()) {
            // Stop BasePool thread and disconnect all connected sockets
            stopPool();

            // Filter for data transmitters that did not send an EOR
            auto data_transmitter_no_eor =
                std::ranges::views::filter(data_transmitter_states_, [](const auto& data_transmitter_p) {
                    return data_transmitter_p.second.state == TransmitterState::BOR_RECEIVED;
                });

            // Note: we do not have a bidirectional range so the content needs to be copied to a vector
            const auto data_transmitter_no_eor_str =
                range_to_string(std::ranges::to<std::vector>(std::ranges::views::keys(data_transmitter_no_eor)));
            LOG(cdtp_logger_, WARNING) << "Not all EOR messages received, emitting substitute EOR messages for "
                                       << data_transmitter_no_eor_str;

            for(const auto& data_transmitter : data_transmitter_no_eor) {
                LOG(cdtp_logger_, DEBUG) << "Creating substitute EOR for " << data_transmitter.first;
                auto run_metadata = Dictionary();
                run_metadata["run_id"] = getRunIdentifier();
                auto condition_code = CDTP::RunCondition::ABORTED;
                if(data_transmitter.second.missed > 0) {
                    condition_code |= CDTP::RunCondition::INCOMPLETE;
                }
                run_metadata["condition_code"] = condition_code;
                run_metadata["condition"] = enum_name(condition_code);
                receive_eor({data_transmitter.first, 0, CDTP1Message::Type::EOR}, std::move(run_metadata));
            }

            throw RecvTimeoutError("EOR messages missing from " + data_transmitter_no_eor_str, data_eor_timeout_);
        }

        // Wait a bit before re-locking
        std::this_thread::sleep_for(50ms);
    }

    LOG(cdtp_logger_, DEBUG) << "All EOR messages received";

    // Stop BasePool thread and disconnect all connected sockets
    stopPool();
}

void ReceiverSatellite::interrupting_receiver() {
    // Stop as usual but do not throw if not all EOR messages received
    try {
        stopping_receiver();
    } catch(const RecvTimeoutError& e) {
        LOG(cdtp_logger_, WARNING) << e.what();
    }
}

void ReceiverSatellite::failure_receiver() {
    // Stop BasePool thread and disconnect all connected sockets
    stopPool();
}

void ReceiverSatellite::reset_data_transmitter_states() {
    const std::lock_guard data_transmitter_states_lock {data_transmitter_states_mutex_};
    data_transmitter_states_.clear();
    for(const auto& data_transmitter : data_transmitters_) {
        data_transmitter_states_.emplace(data_transmitter, TransmitterStateSeq(TransmitterState::NOT_CONNECTED, 0, 0));
    }
}

void ReceiverSatellite::handle_cdtp_message(CDTP1Message&& message) {
    using enum CDTP1Message::Type;
    switch(message.getHeader().getType()) {
    case BOR: {
        LOG(cdtp_logger_, DEBUG) << "Received BOR message from " << message.getHeader().getSender();
        handle_bor_message(std::move(message));
        break;
    }
    [[likely]] case DATA: {
        LOG(cdtp_logger_, TRACE) << "Received data message " << message.getHeader().getSequenceNumber() << " from "
                                 << message.getHeader().getSender();
        bytes_received_ += message.countPayloadBytes();
        handle_data_message(std::move(message));
        break;
    }
    case EOR: {
        LOG(cdtp_logger_, DEBUG) << "Received EOR message from " << message.getHeader().getSender();
        handle_eor_message(std::move(message));
        break;
    }
    default: std::unreachable();
    }
}

void ReceiverSatellite::handle_bor_message(CDTP1Message bor_message) {
    std::unique_lock data_transmitter_states_lock {data_transmitter_states_mutex_};
    auto data_transmitter_it = data_transmitter_states_.find(bor_message.getHeader().getSender());
    // Check that transmitter is not connected yet
    if(data_transmitter_it->second.state != TransmitterState::NOT_CONNECTED) [[unlikely]] {
        throw InvalidCDTPMessageType(CDTP1Message::Type::BOR, "already received BOR");
    }
    data_transmitter_it->second.state = TransmitterState::BOR_RECEIVED;
    data_transmitter_states_lock.unlock();

    receive_bor(bor_message.getHeader(), {Dictionary::disassemble(bor_message.getPayload().at(0)), true});
}

void ReceiverSatellite::handle_data_message(CDTP1Message data_message) {
    std::unique_lock data_transmitter_states_lock {data_transmitter_states_mutex_};
    const auto data_transmitter_it = data_transmitter_states_.find(data_message.getHeader().getSender());
    // Check that BOR was received
    if(data_transmitter_it->second.state != TransmitterState::BOR_RECEIVED) [[unlikely]] {
        throw InvalidCDTPMessageType(CDTP1Message::Type::DATA, "did not receive BOR");
    }
    // Store sequence number and missed messages
    data_transmitter_it->second.missed += data_message.getHeader().getSequenceNumber() - 1 - data_transmitter_it->second.seq;
    data_transmitter_it->second.seq = data_message.getHeader().getSequenceNumber();
    data_transmitter_states_lock.unlock();

    receive_data(std::move(data_message));
}

void ReceiverSatellite::handle_eor_message(CDTP1Message eor_message) {
    std::unique_lock data_transmitter_states_lock {data_transmitter_states_mutex_};
    auto data_transmitter_it = data_transmitter_states_.find(eor_message.getHeader().getSender());
    // Check that BOR was received
    if(data_transmitter_it->second.state != TransmitterState::BOR_RECEIVED) [[unlikely]] {
        throw InvalidCDTPMessageType(CDTP1Message::Type::EOR, "did not receive BOR");
    }
    auto metadata = Dictionary::disassemble(eor_message.getPayload().at(0));

    // Mark run as incomplete if there are missed messages:
    if(data_transmitter_it->second.missed > 0) {
        auto condition_code = CDTP::RunCondition::INCOMPLETE;
        auto condition_code_it = metadata.find("condition_code");
        if(condition_code_it != metadata.end()) {
            // Add INCOMPLETE flag to existing condition
            condition_code |= condition_code_it->second.get<CDTP::RunCondition>();
            condition_code_it->second = condition_code;
        } else {
            metadata.emplace("condition_code", condition_code);
        }
        // Overwrite existing human-readable run condition
        metadata.insert_or_assign("condition", enum_name(condition_code));
    }

    data_transmitter_it->second.state = TransmitterState::EOR_RECEIVED;
    data_transmitter_states_lock.unlock();

    receive_eor(eor_message.getHeader(), std::move(metadata));
}
