/**
 * @file
 * @brief Log Notification Implementation
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Notifications.hpp"

#include <string_view>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/pools/SubscriberPool.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"

using namespace constellation::chirp;
using namespace constellation::listener;
using namespace constellation::message;
using namespace constellation::log;
using namespace constellation::pools;
using namespace constellation::protocol;

Notifications::Notifications(bool log_notifications)
    : SubscriberPool<CMDP1Notification, CHIRP::MONITORING>(
          "NOTIF", [this](auto&& arg) { process_message(std::forward<decltype(arg)>(arg)); }),
      log_notifications_(log_notifications), logger_("NOTIF") {}

std::map<std::string, std::string> Notifications::getTopics(std::string_view sender) {
    const std::lock_guard topics_lock {topics_mutex_};
    const auto sender_it = topics_.find(sender);
    if(sender_it != topics_.end()) {
        return sender_it->second;
    }

    return {};
}

void Notifications::process_message(CMDP1Notification&& msg) {

    // TODO(simonspa) once we have the proper name from CHIRP, we also need to clear cached topics for disconnected senders

    const auto topics = msg.getTopics();
    const auto sender = msg.getHeader().getSender();
    LOG(logger_, DEBUG) << sender << " offers the following topics:";

    const std::lock_guard topics_lock {topics_mutex_};
    const auto [it, inserted] = topics_.emplace(sender, std::map<std::string, std::string>());

    if(!inserted) {
        it->second.clear();
    }

    for(const auto& t : topics) {
        it->second.emplace(t.first, t.second.str());
        LOG(logger_, DEBUG) << "\t" << t.first;
    }
}

void Notifications::host_connected(const DiscoveredService& service) {
    // Subscribe to notification topic:
    subscribe(service.host_id, (log_notifications_ ? "LOG?" : "STAT?"));
}
