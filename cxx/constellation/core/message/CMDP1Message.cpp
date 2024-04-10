/**
 * @file
 * @brief Implementation of CMDP1 message type
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "CMDP1Message.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <magic_enum.hpp>
#include <msgpack.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/std23.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::utils;
using namespace std::literals::string_literals;

CMDP1Message::CMDP1Message(std::string topic, CMDP1Message::Header header, std::shared_ptr<zmq::message_t> payload)
    : topic_(std::move(topic)), header_(std::move(header)), payload_(std::move(payload)) {}

bool CMDP1Message::isLogMessage() const {
    return topic_.starts_with("LOG/");
}

zmq::multipart_t CMDP1Message::assemble() {
    zmq::multipart_t frames {};

    // First frame: topic
    frames.addstr(topic_);

    // Second frame: header
    msgpack::sbuffer sbuf_header {};
    msgpack::pack(sbuf_header, header_);
    frames.addmem(sbuf_header.data(), sbuf_header.size());

    // Third frame: payload
    zmq::message_t payload_frame {};
    payload_frame.swap(*payload_);
    frames.add(std::move(payload_frame));

    return frames;
}

CMDP1Message CMDP1Message::disassemble(zmq::multipart_t& frames) {
    if(frames.size() != 3) {
        throw MessageDecodingError("Invalid number of message frames");
    }

    // Decode topic
    const auto topic = frames.at(0).to_string();
    if(!(topic.starts_with("LOG/") || topic.starts_with("STAT/"))) {
        throw MessageDecodingError("Invalid message topic, neither log or statistics message");
    }

    // Check if valid log level by trying to decode it
    if(topic.starts_with("LOG/")) {
        getLogLevelFromTopic(topic);
    }

    // Decode header
    const auto header = Header::disassemble({to_byte_ptr(frames.at(1).data()), frames.at(1).size()});

    // Decode payload
    auto payload = std::make_shared<zmq::message_t>();
    frames.at(2).swap(*payload);

    // Create message
    return {topic, header, std::move(payload)};
}

Level CMDP1Message::getLogLevelFromTopic(std::string_view topic) {
    if(!topic.starts_with("LOG/")) {
        throw MessageDecodingError("Not a log message");
    }

    // Search for second slash after "LOG/" to get substring with log level
    const auto level_endpos = topic.find_first_of('/', 4);
    const auto level_str = topic.substr(4, level_endpos - 4);
    const auto level_opt = magic_enum::enum_cast<Level>(level_str);
    if(!level_opt.has_value()) {
        throw MessageDecodingError("\"" + to_string(level_str) + "\" is not a valid log level");
    }

    return level_opt.value();
}

CMDP1LogMessage::CMDP1LogMessage(Level level, std::string log_topic, CMDP1Message::Header header, std::string_view message)
    : CMDP1Message("LOG/" + to_string(level) + (log_topic.empty() ? ""s : "/" + transform(log_topic, ::toupper)),
                   std::move(header),
                   std::make_shared<zmq::message_t>(message)),
      level_(level), log_topic_(std::move(log_topic)) {}

CMDP1LogMessage::CMDP1LogMessage(CMDP1Message message) : CMDP1Message(std::move(message)) {
    if(!isLogMessage()) {
        throw IncorrectMessageType("Not a log message");
    }

    const auto topic = getTopic();
    level_ = getLogLevelFromTopic(topic);

    // Search for a '/' after "LOG/"
    const auto level_endpos = topic.find_first_of('/', 4);
    // If a '/' was found, then topic is formatted as "LOG/LEVEL/TOPIC", else the log topic is empty
    if(level_endpos != std::string::npos) {
        log_topic_ = topic.substr(level_endpos + 1);
    }
}

std::string_view CMDP1LogMessage::getLogMessage() const {
    return getPayload()->to_string_view();
}

CMDP1LogMessage CMDP1LogMessage::disassemble(zmq::multipart_t& frames) {
    // Use disassemble from base class and cast via constructor
    return {CMDP1Message::disassemble(frames)};
}
