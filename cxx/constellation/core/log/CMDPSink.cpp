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
#include <iomanip>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

#include <magic_enum.hpp>
#include <spdlog/async_logger.h>
#include <spdlog/details/log_msg.h>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/log/SinkManager.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/utils/networking.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/core/utils/windows.hpp"

using namespace constellation;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::utils;
using namespace std::chrono_literals;

namespace {
    // Find path relative to cxx/, otherwise path without any parent
    std::string get_rel_file_path(std::string file_path_char) {
        auto file_path = to_platform_string(std::move(file_path_char));
        const auto src_dir = std::filesystem::path::preferred_separator + to_platform_string("cxx") +
                             std::filesystem::path::preferred_separator;
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
} // namespace

CMDPSink::CMDPSink(std::shared_ptr<zmq::context_t> context)
    : context_(std::move(context)), pub_socket_(*context_, zmq::socket_type::xpub), port_(bind_ephemeral_port(pub_socket_)) {
    // Set reception timeout for subscription messages on XPUB socket to zero because we need to mutex-lock the socket
    // while reading and cannot log at the same time.
    pub_socket_.set(zmq::sockopt::rcvtimeo, 0);
}

CMDPSink::~CMDPSink() {
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
        auto received = recv_msg.recv(pub_socket_);

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
        LOG(*logger_, TRACE) << "Received " << (subscribe ? "" : "un") << "subscribe message for " << body;

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
            LOG(*logger_, TRACE) << "Invalid log level " << std::quoted(level_str) << ", ignoring";
            continue;
        }

        const auto topic = (level_endpos != std::string::npos ? body.substr(level_endpos + 1) : std::string_view());
        const auto topic_uc = transform(topic, ::toupper);
        LOG(*logger_, TRACE) << "In/decrementing subscription counters for topic " << std::quoted(topic_uc);

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
            auto it = std::ranges::find_if(levels, [](const auto& i) { return i.second > 0; });
            if(it != levels.end()) {
                if(!logger.empty()) {
                    cmdp_sub_topic_levels[logger] = it->first;
                } else {
                    cmdp_global_level = it->first;
                }
            }
        }

        LOG(*logger_, TRACE) << "Lowest global log level: " << std::quoted(to_string(cmdp_global_level));

        // Update subscriptions
        SinkManager::getInstance().updateCMDPLevels(cmdp_global_level, std::move(cmdp_sub_topic_levels));
    }
}

void CMDPSink::enableSending(std::string sender_name) {
    sender_name_ = std::move(sender_name);

    // Get CMDP logger
    logger_ = std::make_unique<Logger>("CMDP");

    // Start thread monitoring the socket for subscription messages
    subscription_thread_ = std::jthread(std::bind_front(&CMDPSink::subscription_loop, this));

    // Register service in CHIRP
    auto* chirp_manager = chirp::Manager::getDefaultInstance();
    if(chirp_manager != nullptr) {
        chirp_manager->registerService(chirp::MONITORING, port_);
    } else {
        LOG(*logger_, WARNING) << "Failed to advertise logging on the network, satellite might not be discovered";
    }
    LOG(*logger_, INFO) << "Starting to log on port " << port_;
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

    // Create and send CMDP message
    CMDP1LogMessage(from_spdlog_level(msg.level),
                    to_string(msg.logger_name), // NOLINT(misc-include-cleaner) might be fmt string
                    std::move(msghead),
                    to_string(msg.payload))
        .assemble()
        .send(pub_socket_);
}
