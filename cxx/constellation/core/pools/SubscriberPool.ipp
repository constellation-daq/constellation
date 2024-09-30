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
#include <iomanip>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

#include <zmq.hpp>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"

namespace constellation::pools {

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    SubscriberPool<MESSAGE, SERVICE>::SubscriberPool(std::string_view log_topic,
                                                     std::function<void(MESSAGE&&)> callback)
        : BasePoolT(log_topic, std::move(callback)) {}

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::setSubscriptionTopics(std::set<std::string> topics) {
        default_topics_ = std::move(topics);
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::scribe(std::string_view host, std::string_view topic, bool subscribe) {
        // Get host ID from name:
        const auto host_id = message::MD5Hash(host);

        const std::lock_guard sockets_lock {BasePoolT::sockets_mutex_};

        const auto socket_it = std::ranges::find_if(
            BasePoolT::get_sockets(), host_id, [&](const auto& s) { return s.first.host_id == host_id; });
        if(socket_it != BasePoolT::get_sockets().end()) {
            if(subscribe) {
                LOG(BasePoolT::pool_logger_, TRACE) << "Subscribing to " << std::quoted(topic);
                socket_it->second.set(zmq::sockopt::subscribe, topic);
            } else {
                LOG(BasePoolT::pool_logger_, TRACE) << "Unsubscribing from " << std::quoted(topic);
                socket_it->second.set(zmq::sockopt::unsubscribe, topic);
            }
        }
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::scribe_all(std::string_view topic, bool subscribe) {
        const std::lock_guard sockets_lock {BasePoolT::sockets_mutex_};

        for(auto& [host, socket] : BasePoolT::get_sockets()) {
            if(subscribe) {
                LOG(BasePoolT::pool_logger_, TRACE) << "Subscribing to " << std::quoted(topic);
                socket.set(zmq::sockopt::subscribe, topic);
            } else {
                LOG(BasePoolT::pool_logger_, TRACE) << "Unsubscribing from " << std::quoted(topic);
                socket.set(zmq::sockopt::unsubscribe, topic);
            }
        }
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::socket_connected(zmq::socket_t& socket) {
        // Directly subscribe to default topic list
        for(const auto& topic : default_topics_) {
            socket.set(zmq::sockopt::subscribe, topic);
        }
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::subscribe(std::string_view host, std::string_view topic) {
        scribe(host, topic, true);
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::unsubscribe(std::string_view host, std::string_view topic) {
        scribe(host, topic, false);
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::subscribe(std::string_view topic) {
        scribe_all(topic, true);
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::unsubscribe(std::string_view topic) {
        scribe_all(topic, false);
    }

} // namespace constellation::pools
