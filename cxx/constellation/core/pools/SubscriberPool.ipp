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
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include <zmq.hpp>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"

namespace constellation::pools {

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    SubscriberPool<MESSAGE, SERVICE>::SubscriberPool(std::string_view log_topic, std::function<void(MESSAGE&&)> callback)
        : BasePoolT(log_topic, std::move(callback)) {}

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::scribe(std::string_view host, std::string_view topic, bool subscribe) {
        // Get host ID from name
        const auto host_id = message::MD5Hash(host);

        const std::lock_guard sockets_lock {BasePoolT::sockets_mutex_};

        const auto socket_it = std::ranges::find(
            BasePoolT::get_sockets(), host_id, [&](const auto& socket_p) { return socket_p.first.host_id; });
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
    void SubscriberPool<MESSAGE, SERVICE>::socket_connected(const chirp::DiscoveredService& service, zmq::socket_t& socket) {
        const std::lock_guard subscribed_topics_lock {subscribed_topics_mutex_};

        // Directly subscribe to current topic list
        for(const auto& topic : subscribed_topics_) {
            socket.set(zmq::sockopt::subscribe, topic);
        }
        // If extra topics for host, also subscribe to those
        const auto host_it = std::ranges::find(
            extra_subscribed_topics_, service.host_id, [&](const auto& host_p) { return message::MD5Hash(host_p.first); });
        if(host_it != extra_subscribed_topics_.end()) {
            std::ranges::for_each(host_it->second, [&](const auto& topic) {
                if(!subscribed_topics_.contains(topic)) {
                    scribe(host_it->first, topic, true);
                }
            });
        }
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::setSubscriptionTopics(std::set<std::string> topics) {
        const std::lock_guard subscribed_topics_lock {subscribed_topics_mutex_};

        // Set of topics to unsubscribe: current topics not in new topics
        std::set<std::string_view> to_unsubscribe {};
        std::ranges::for_each(subscribed_topics_, [&](const auto& topic) {
            if(!topics.contains(topic)) {
                to_unsubscribe.emplace(topic);
            }
        });
        // Set of topics to subscribe: new topics not in current topics
        std::set<std::string_view> to_subscribe {};
        std::ranges::for_each(topics, [&](const auto& new_topic) {
            if(!subscribed_topics_.contains(new_topic)) {
                to_subscribe.emplace(new_topic);
            }
        });
        // Unsubscribe from old topics
        std::ranges::for_each(to_unsubscribe, [&](const auto& topic) { scribe_all(topic, false); });
        // Subscribe to new topics
        std::ranges::for_each(to_subscribe, [&](const auto& topic) { scribe_all(topic, true); });

        // Check if extra topics contained unsubscribed topics, if so subscribe again
        std::ranges::for_each(extra_subscribed_topics_, [&](const auto& host_p) {
            std::ranges::for_each(host_p.second, [&](const auto& topic) {
                if(to_unsubscribe.contains(topic)) {
                    scribe(host_p.first, topic, true);
                }
            });
        });

        // Store new set of subscribed topics
        subscribed_topics_ = std::move(topics);
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::subscribe(std::string topic) {
        std::set<std::string> new_subscribed_topics {};
        {
            // Copy current topics
            const std::lock_guard subscribed_topics_lock {subscribed_topics_mutex_};
            new_subscribed_topics = subscribed_topics_;
        }
        // Emplace new topic
        const auto [_, inserted] = new_subscribed_topics.emplace(std::move(topic));
        // Handle logic in setSubscriptionTopics
        if(inserted) [[likely]] {
            setSubscriptionTopics(std::move(new_subscribed_topics));
        }
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::unsubscribe(const std::string& topic) {
        std::set<std::string> new_subscribed_topics {};
        {
            // Copy current topics
            const std::lock_guard subscribed_topics_lock {subscribed_topics_mutex_};
            new_subscribed_topics = subscribed_topics_;
        }
        // Erase requested topic
        const auto erased = new_subscribed_topics.erase(topic);
        // Handle logic in setSubscriptionTopics
        if(erased > 0) [[likely]] {
            setSubscriptionTopics(std::move(new_subscribed_topics));
        }
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::setExtraSubscriptionTopics(const std::string& host,
                                                                      std::set<std::string> topics) {
        const std::lock_guard subscribed_topics_lock {subscribed_topics_mutex_};

        // Check if extra topics already set
        const auto host_it = extra_subscribed_topics_.find(host);
        if(host_it != extra_subscribed_topics_.end()) {
            // Set of topics to unsubscribe: current topics not in subscribed_topics or new topics
            std::set<std::string_view> to_unsubscribe {};
            std::ranges::for_each(host_it->second, [&](const auto& topic) {
                if(!subscribed_topics_.contains(topic) || !topics.contains(topic)) {
                    to_unsubscribe.emplace(topic);
                }
            });
            // Set of topics to subscribe: new topics not in subscribed_topics or current topics
            std::set<std::string_view> to_subscribe {};
            std::ranges::for_each(topics, [&](const auto& new_topic) {
                if(!subscribed_topics_.contains(new_topic) || !host_it->second.contains(new_topic)) {
                    to_subscribe.emplace(new_topic);
                }
            });
            // Unsubscribe from old topics
            std::ranges::for_each(to_unsubscribe, [&](const auto& topic) { scribe(host, topic, false); });
            // Subscribe to new topics
            std::ranges::for_each(to_subscribe, [&](const auto& topic) { scribe(host, topic, true); });
        } else {
            // Subscribe to each topic not present in subscribed_topics_
            std::ranges::for_each(topics, [&](const auto& topic) {
                if(!subscribed_topics_.contains(topic)) {
                    scribe(host, topic, true);
                }
            });
        }

        // Store new set of extra topics
        extra_subscribed_topics_[host] = std::move(topics);
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::subscribeExtra(const std::string& host, std::string topic) {
        std::set<std::string> new_subscription_topics {};
        bool run_logic {true};
        {
            const std::lock_guard subscribed_topics_lock {subscribed_topics_mutex_};
            const auto host_it = extra_subscribed_topics_.find(host);
            if(host_it != extra_subscribed_topics_.end()) {
                // Copy current topics and emplace new topic
                new_subscription_topics = host_it->second;
                const auto [_, inserted] = new_subscription_topics.emplace(std::move(topic));
                run_logic = inserted;
            } else {
                // No topics yet, add new topic
                new_subscription_topics.emplace(std::move(topic));
            }
        }
        // Handle logic in setExtraSubscriptionTopics
        if(run_logic) [[likely]] {
            setExtraSubscriptionTopics(host, std::move(new_subscription_topics));
        }
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::unsubscribeExtra(const std::string& host, const std::string& topic) {
        std::set<std::string> new_subscription_topics {};
        bool run_logic {false};
        {
            const std::lock_guard subscribed_topics_lock {subscribed_topics_mutex_};
            const auto host_it = extra_subscribed_topics_.find(host);
            if(host_it != extra_subscribed_topics_.end()) {
                // Copy current topics and erase requested topic
                new_subscription_topics = host_it->second;
                const auto erased = new_subscription_topics.erase(topic);
                run_logic = erased > 0;
            }
        }
        // Handle logic in setExtraSubscriptionTopics
        if(run_logic) [[likely]] {
            setExtraSubscriptionTopics(host, std::move(new_subscription_topics));
        }
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::removeExtraSubscriptions(const std::string& host) {
        const std::lock_guard subscribed_topics_lock {subscribed_topics_mutex_};
        const auto host_it = extra_subscribed_topics_.find(host);
        if(host_it != extra_subscribed_topics_.end()) {
            // Unscribe from each topic not in subscribed_topics_
            std::for_each(host_it->second, [&](const auto& topic) {
                if(!subscribed_topics_.contains(topic)) {
                    scribe(host, topic, false);
                }
            });
            extra_subscribed_topics_.erase(host_it);
        }
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    void SubscriberPool<MESSAGE, SERVICE>::removeExtraSubscriptions() {
        const std::lock_guard subscribed_topics_lock {subscribed_topics_mutex_};
        std::for_each(extra_subscribed_topics_, [&](const auto& host_p) {
            // Unscribe from each topic not in subscribed_topics_
            std::for_each(host_p.second, [&](const auto& topic) {
                if(!subscribed_topics_.contains(topic)) {
                    scribe(host_p.first, topic, false);
                }
            });
        });
        extra_subscribed_topics_.clear();
    }

} // namespace constellation::pools
