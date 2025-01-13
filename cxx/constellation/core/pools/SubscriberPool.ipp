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
#include <iomanip>
#include <mutex>
#include <string_view>
#include <utility>

#include <zmq.hpp>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/networking/exceptions.hpp"

namespace constellation::pools {

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    SubscriberPool<MESSAGE, SERVICE>::SubscriberPool(std::string_view log_topic, std::function<void(MESSAGE&&)> callback)
        : BasePoolT(log_topic, std::move(callback)) {}

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::scribe(message::MD5Hash host_id, std::string_view topic, bool subscribe) {
        try {
            const std::lock_guard sockets_lock {BasePoolT::sockets_mutex_};
            const auto socket_it = std::ranges::find(
                BasePoolT::get_sockets(), host_id, [&](const auto& socket_p) { return socket_p.first.host_id; });
            if(socket_it != BasePoolT::get_sockets().end()) {
                if(subscribe) {
                    LOG(BasePoolT::pool_logger_, TRACE)
                        << "Subscribing to " << std::quoted(topic) << " for " << socket_it->first.to_uri();
                    socket_it->second.set(zmq::sockopt::subscribe, topic);
                } else {
                    LOG(BasePoolT::pool_logger_, TRACE)
                        << "Unsubscribing from " << std::quoted(topic) << " for " << socket_it->first.to_uri();
                    socket_it->second.set(zmq::sockopt::unsubscribe, topic);
                }
            }
        } catch(const zmq::error_t& e) {
            throw networking::NetworkError(e.what());
        }
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::scribe_all(std::string_view topic, bool subscribe) {

        try {
            const std::lock_guard sockets_lock {BasePoolT::sockets_mutex_};
            for(auto& [host, socket] : BasePoolT::get_sockets()) {
                if(subscribe) {
                    LOG(BasePoolT::pool_logger_, TRACE)
                        << "Subscribing to " << std::quoted(topic) << " for " << host.to_uri();
                    socket.set(zmq::sockopt::subscribe, topic);
                } else {
                    LOG(BasePoolT::pool_logger_, TRACE)
                        << "Unsubscribing from " << std::quoted(topic) << " for " << host.to_uri();
                    socket.set(zmq::sockopt::unsubscribe, topic);
                }
            }
        } catch(const zmq::error_t& e) {
            throw networking::NetworkError(e.what());
        }
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::subscribe(std::string_view host, std::string_view topic) {
        subscribe(message::MD5Hash(host), topic);
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::subscribe(message::MD5Hash host_id, std::string_view topic) {
        scribe(host_id, topic, true);
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::subscribe(std::string_view topic) {
        scribe_all(topic, true);
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::unsubscribe(std::string_view host, std::string_view topic) {
        unsubscribe(message::MD5Hash(host), topic);
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::unsubscribe(message::MD5Hash host_id, std::string_view topic) {
        scribe(host_id, topic, false);
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::unsubscribe(std::string_view topic) {
        scribe_all(topic, false);
    }

} // namespace constellation::pools
