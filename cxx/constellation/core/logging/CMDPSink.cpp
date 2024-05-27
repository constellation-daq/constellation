/**
 * @file
 * @brief Implementation of CMDPSink
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "CMDPSink.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include <spdlog/details/log_msg.h>
#include <zmq.hpp>

#include "constellation/core/logging/Level.hpp"
#include "constellation/core/logging/SinkManager.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/utils/ports.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/core/utils/windows.hpp"

using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::utils;
using namespace std::literals::chrono_literals;

// Find path relative to cxx/, otherwise path without any parent
std::string get_rel_file_path(std::string file_path_char) {
    auto file_path = to_platform_string(std::move(file_path_char));
    const auto src_dir =
        std::filesystem::path::preferred_separator + to_platform_string("cxx") + std::filesystem::path::preferred_separator;
    const auto src_dir_pos = file_path.find(src_dir);
    if(src_dir_pos != std::filesystem::path::string_type::npos) {
        // found /cxx/, start path after pattern
        file_path = file_path.substr(src_dir_pos + src_dir.length());
    } else {
        // try to find last / for filename
        const auto file_pos = file_path.find_last_of(std::filesystem::path::preferred_separator);
        if(file_pos != std::filesystem::path::string_type::npos) {
            file_path = file_path.substr(file_pos + 1);
        }
    }
    return to_std_string(std::move(file_path));
}

// Bind socket to ephemeral port on construction
CMDPSink::CMDPSink() : publisher_(context_, zmq::socket_type::xpub), port_(bind_ephemeral_port(publisher_)) {
    // Set reception timeout for subscription messages on XPUB socket
    publisher_.set(zmq::sockopt::rcvtimeo, static_cast<int>(std::chrono::milliseconds(300).count()));
    // Start thread monitoring the socket for subscription messages
    subscription_thread_ = std::jthread(std::bind_front(&CMDPSink::loop, this));
}

CMDPSink::~CMDPSink() {
    subscription_thread_.request_stop();
    if(subscription_thread_.joinable()) {
        subscription_thread_.join();
    }
}

void CMDPSink::loop(const std::stop_token& stop_token) {
    while(!stop_token.stop_requested()) {
        zmq::multipart_t recv_msg {};
        auto received = recv_msg.recv(publisher_);

        // Return if timed out or wrong number of frames received:
        if(!received || recv_msg.size() != 1) {
            continue;
        }

        const auto& frame = recv_msg.front();

        // First byte \x01 is subscription, \0x00 is unsubscription
        const auto subscribe = static_cast<bool>(*frame.data<uint8_t>());

        const auto topic = frame.to_string_view();

        // TODO(simonspa) At some point we also have to treat STAT here
        // FIXME we need to allow also without `/` or empty subscriptions?
        if(!topic.starts_with("LOG/")) {
            continue;
        }

        const auto level_endpos = topic.find_first_of('/', 4);
        const auto level_str = topic.substr(4, level_endpos - 4);

        // Empty level means subscription to everything
        const auto level = (level_str.empty() ? std::optional<Level>(TRACE) : magic_enum::enum_cast<Level>(level_str));

        // Only accept valid levels
        if(!level.has_value()) {
            continue;
        }

        // FIXME this only subscribes and only sets the global level not topics
        if(subscribe) {
            SinkManager::getInstance().setCMDPLevelsCustom(level.value());
        }
    }
}

void CMDPSink::setSender(std::string sender_name) {
    sender_name_ = std::move(sender_name);
}

void CMDPSink::sink_it_(const spdlog::details::log_msg& msg) {

    // At the very beginning we wait 500ms before starting the async logging.
    // This way the socket can fetch already pending subscriptions
    std::call_once(setup_flag_, []() { std::this_thread::sleep_for(500ms); });

    // Create message header
    auto msghead = CMDP1Message::Header(sender_name_, msg.time);
    // Add source and thread information only at TRACE level:
    if(from_spdlog_level(msg.level) <= TRACE) {
        msghead.setTag("thread", static_cast<std::int64_t>(msg.thread_id));
        // Add log source if not empty
        if(!msg.source.empty()) {
            msghead.setTag("filename", get_rel_file_path(msg.source.filename));
            msghead.setTag("lineno", static_cast<std::int64_t>(msg.source.line));
            msghead.setTag("funcname", msg.source.funcname);
        }
    }

    // Create and send CMDP message
    CMDP1LogMessage(from_spdlog_level(msg.level), to_string(msg.logger_name), std::move(msghead), to_string(msg.payload))
        .assemble()
        .send(publisher_);
}
