/**
 * @file
 * @brief Subscriber pool implementation
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "SubscriberPool.hpp" // NOLINT(misc-header-include-cycle)

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

#include <zmq.hpp>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/chirp/Manager.hpp"

namespace constellation::pools {

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    SubscriberPool<MESSAGE, SERVICE>::SubscriberPool(std::string_view log_topic,
                                                     std::function<void(MESSAGE&&)> callback,
                                                     std::initializer_list<std::string> default_topics)
        : BasePoolT(log_topic, std::move(callback)), default_topics_(default_topics) {}

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::socket_connected(const chirp::DiscoveredService& /*service*/,
                                                            zmq::socket_t& socket) {
        // Directly subscribe to default topic list
        for(const auto& topic : default_topics_) {
            socket.set(zmq::sockopt::subscribe, topic);
        }
    }

} // namespace constellation::pools
