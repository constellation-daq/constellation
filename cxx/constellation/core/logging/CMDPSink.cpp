/**
 * @file
 * @brief Implementation of CMDPSink
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "CMDPSink.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

#include <magic_enum.hpp>
#include <spdlog/details/log_msg.h>
#include <zmq.hpp>
#include <zmq_addon.hpp>

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
    // Set reception timeout for subscription messages on XPUB socket to zero because we need to mutex-lock the socket
    // while reading and cannot log at the same time.
    publisher_.set(zmq::sockopt::rcvtimeo, 0);
}

CMDPSink::~CMDPSink() {
    send_thread_.request_stop();
    if(send_thread_.joinable()) {
        send_thread_.join();
    }
    subscription_thread_.request_stop();
    if(subscription_thread_.joinable()) {
        subscription_thread_.join();
    }
}

void CMDPSink::subscription_loop(const std::stop_token& stop_token) {
    while(!stop_token.stop_requested()) {

        // Lock for the mutex provided by the sink base class
        std::unique_lock socket_lock {mutex_};

        // Receive subscription message
        zmq::multipart_t recv_msg {};
        auto received = recv_msg.recv(publisher_);

        socket_lock.unlock();

        // Return if timed out or wrong number of frames received:
        if(!received || recv_msg.size() != 1) {
            // Only check every 300ms for new subscription messages:
            std::this_thread::sleep_for(300ms);
            continue;
        }

        const auto& frame = recv_msg.front();

        // First byte \x01 is subscription, \0x00 is unsubscription
        const auto subscribe = static_cast<bool>(*frame.data<uint8_t>());

        // Log topic is message body stripped by first byte
        auto body = frame.to_string_view();
        body.remove_prefix(1);

        // TODO(simonspa) At some point we also have to treat STAT here
        if(!body.starts_with("LOG/")) {
            continue;
        }

        const auto level_endpos = body.find_first_of('/', 4);
        const auto level_str = body.substr(4, level_endpos - 4);

        // Empty level means subscription to everything
        const auto level = (level_str.empty() ? std::optional<Level>(TRACE)
                                              : magic_enum::enum_cast<Level>(level_str, magic_enum::case_insensitive));

        // Only accept valid levels
        if(!level.has_value()) {
            continue;
        }

        const auto topic = (level_endpos != std::string::npos ? body.substr(level_endpos + 1) : std::string_view());
        const auto topic_uc = transform(topic, ::toupper);
        if(subscribe) {
            log_subscriptions_[topic_uc][level.value()] += 1;
        } else {
            if(log_subscriptions_[topic_uc][level.value()] > 0) {
                log_subscriptions_[topic_uc][level.value()] -= 1;
            }
        }

        // Figure out lowest level for each topic
        auto cmdp_global_level = Level::OFF;
        std::map<std::string_view, Level> cmdp_sub_topic_levels;
        for(const auto& [logger, levels] : log_subscriptions_) {
            auto it = std::find_if(std::begin(levels), std::end(levels), [](const auto& i) { return i.second > 0; });
            if(it != std::end(levels)) {
                if(!logger.empty()) {
                    cmdp_sub_topic_levels[logger] = it->first;
                } else {
                    cmdp_global_level = it->first;
                }
            }
        }

        // Update subscriptions
        SinkManager::getInstance().updateCMDPLevels(cmdp_global_level, std::move(cmdp_sub_topic_levels));
    }
}

void CMDPSink::enableSending(std::string sender_name) {
    sender_name_ = std::move(sender_name);

    // Start thread monitoring the socket for subscription messages
    subscription_thread_ = std::jthread(std::bind_front(&CMDPSink::subscription_loop, this));

    // Replace sender name for already queued messages
    std::unique_lock msg_queue_lock {msg_queue_mutex_};
    for(auto& msg : msg_queue_) {
        msg->setSender(sender_name_);
    }
    msg_queue_lock.unlock();

    // We wait a bit before starting to send message, this way the socket can fetch already pending subscriptions
    std::this_thread::sleep_for(300ms);

    // Start send thread and notify for already queued messages
    send_thread_ = std::jthread(std::bind_front(&CMDPSink::send_loop, this));
    msg_queue_cv_.notify_one();
}

void CMDPSink::send_loop(const std::stop_token& stop_token) {
    while(!stop_token.stop_requested()) {
        // Wait for notification
        std::unique_lock msg_queue_lock {msg_queue_mutex_};
        if(!msg_queue_cv_.wait(msg_queue_lock, stop_token, [&]() { return !msg_queue_.empty(); })) {
            // Stop was requested and no messages queued
            break;
        }
        // Send all messages in queue
        while(!msg_queue_.empty()) {
            msg_queue_.front()->assemble().send(publisher_);
            msg_queue_.pop_front();
        }
    }
}

void CMDPSink::sink_it_(const spdlog::details::log_msg& msg) {
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

    // Create and queue CMDP message
    auto cmdp_msg = std::make_unique<CMDP1LogMessage>(
        from_spdlog_level(msg.level), to_string(msg.logger_name), std::move(msghead), to_string(msg.payload));
    std::unique_lock msg_queue_lock {msg_queue_mutex_};
    msg_queue_.emplace_back(std::move(cmdp_msg));
    msg_queue_lock.unlock();
    msg_queue_cv_.notify_one();
}
