/**
 * @file
 * @brief Log Notification Implementation
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "LogNotifications.hpp"

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

LogNotifications::LogNotifications()
    : SubscriberPool<CMDP1Notification, CHIRP::MONITORING>(
          "NOTIF", [this](auto&& arg) { process_message(std::forward<decltype(arg)>(arg)); }),
      logger_("NOTIF") {}

void LogNotifications::process_message(CMDP1Notification&& msg) {

    LOG(logger_, DEBUG) << msg.getHeader().getSender() << " offers the following log topics:";
    const auto topics = msg.getTopics();
    for(const auto& t : topics) {
        LOG(logger_, DEBUG) << "\t" << t.first;
    }
}

void LogNotifications::host_connected(const DiscoveredService& service) {
    // Subscribe to log notification topic:
    subscribe(service.host_id, "LOG?");
}
