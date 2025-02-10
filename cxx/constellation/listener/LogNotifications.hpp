/**
 * @file
 * @brief Log Notification listener
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/pools/SubscriberPool.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"

namespace constellation::listener {

    class LogNotifications
        : public pools::SubscriberPool<message::CMDP1Notification, protocol::CHIRP::ServiceIdentifier::MONITORING> {
    public:
        using SubscriberPoolT =
            pools::SubscriberPool<message::CMDP1Notification, protocol::CHIRP::ServiceIdentifier::MONITORING>;

        LogNotifications();

    private:
        /**
         * @brief Callback registered for processing log notification messages from the subscription pool
         *
         * @param msg Received notification message
         */
        void process_message(message::CMDP1Notification&& msg);

        void host_connected(const constellation::chirp::DiscoveredService& service) override;

    private:
        // Hide subscribe/unsubscribe functions from SubscriberPool
        using SubscriberPoolT::subscribe;
        using SubscriberPoolT::unsubscribe;

    private:
        /** Logger to use */
        log::Logger logger_;
    };

} // namespace constellation::listener
