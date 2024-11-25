/**
 * @file
 * @brief LogListener implementation
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "LogListener.hpp"

#include <algorithm>
#include <functional>
#include <iomanip>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include <zmq.hpp>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/message/CMDP1Message.hpp"

using namespace constellation;
using namespace constellation::pools;
using namespace constellation::message;

LogListener::LogListener(std::string_view log_topic, std::function<void(CMDP1Message&&)> callback)
    : SubscriberPoolT(log_topic, std::move(callback)) {}

LogListener::~LogListener() = default;

void LogListener::socket_connected(const chirp::DiscoveredService& service, zmq::socket_t& socket) {
    const std::lock_guard subscribed_topics_lock {subscribed_topics_mutex_};

    // Directly subscribe to current topic list
    for(const auto& topic : subscribed_topics_) {
        LOG(pool_logger_, TRACE) << "Subscribing to " << std::quoted(topic) << " for " << service.to_uri();
        socket.set(zmq::sockopt::subscribe, topic);
    }
    // If extra topics for host, also subscribe to those
    const auto host_it = std::ranges::find(
        extra_subscribed_topics_, service.host_id, [&](const auto& host_p) { return MD5Hash(host_p.first); });
    if(host_it != extra_subscribed_topics_.end()) {
        std::ranges::for_each(host_it->second, [&](const auto& topic) {
            if(!subscribed_topics_.contains(topic)) {
                LOG(pool_logger_, TRACE) << "Subscribing to " << std::quoted(topic) << " for " << service.to_uri();
                socket.set(zmq::sockopt::subscribe, topic);
            }
        });
    }
}

LogListener::socket_pair LogListener::find_socket(std::string_view host, sockets_map& sockets) {
    const auto host_id = MD5Hash(host);
    const auto socket_it = std::ranges::find(sockets, host_id, [&](const auto& socket_p) { return socket_p.first.host_id; });
    if(socket_it != sockets.end()) {
        return {socket_it->first, socket_it->second};
    }
    return {};
}

void LogListener::scribe(socket_pair& socket_pair, std::string_view topic, bool subscribe) {
    if(socket_pair.second != nullptr) {
        if(subscribe) {
            LOG(pool_logger_, TRACE) << "Subscribing to " << std::quoted(topic) << " for " << socket_pair.first.to_uri();
            socket_pair.second.set(zmq::sockopt::subscribe, topic);
        } else {
            LOG(pool_logger_, TRACE) << "Unsubscribing from " << std::quoted(topic) << " for " << socket_pair.first.to_uri();
            socket_pair.second.set(zmq::sockopt::unsubscribe, topic);
        }
    }
}

void LogListener::scribe_all(sockets_map& sockets, std::string_view topic, bool subscribe) {
    for(auto& [host, socket] : sockets) {
        if(subscribe) {
            LOG(pool_logger_, TRACE) << "Subscribing to " << std::quoted(topic) << " for " << host.to_uri();
            socket.set(zmq::sockopt::subscribe, topic);
        } else {
            LOG(pool_logger_, TRACE) << "Unsubscribing from " << std::quoted(topic) << " for " << host.to_uri();
            socket.set(zmq::sockopt::unsubscribe, topic);
        }
    }
}

void LogListener::setSubscriptionTopics(std::set<std::string> topics) {
    // Lock sockets_mutex_ before to avoid potential deadlock
    const std::lock_guard sockets_lock {sockets_mutex_};
    auto& sockets = get_sockets();

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
    std::ranges::for_each(to_unsubscribe, [&](const auto& topic) { scribe_all(sockets, topic, false); });
    // Subscribe to new topics
    std::ranges::for_each(to_subscribe, [&](const auto& topic) { scribe_all(sockets, topic, true); });

    // Check if extra topics contained unsubscribed topics, if so subscribe again
    std::ranges::for_each(extra_subscribed_topics_, [&](const auto& host_p) {
        auto socket_pair = find_socket(host_p.first, sockets);
        std::ranges::for_each(host_p.second, [&](const auto& topic) {
            if(to_unsubscribe.contains(topic)) {
                scribe(socket_pair, topic, true);
            }
        });
    });

    // Store new set of subscribed topics
    subscribed_topics_ = std::move(topics);
}

void LogListener::subscribe(std::string topic) {
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

void LogListener::unsubscribe(const std::string& topic) {
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

void LogListener::setExtraSubscriptionTopics(const std::string& host, std::set<std::string> topics) {
    // Lock sockets_mutex_ before to avoid potential deadlock
    const std::lock_guard sockets_lock {sockets_mutex_};
    auto& sockets = get_sockets();
    auto socket_pair = find_socket(host, sockets);

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
        // Set of topics to subscribe: new topics not in subscribed_topics and current topics
        std::set<std::string_view> to_subscribe {};
        std::ranges::for_each(topics, [&](const auto& new_topic) {
            if(!subscribed_topics_.contains(new_topic) && !host_it->second.contains(new_topic)) {
                to_subscribe.emplace(new_topic);
            }
        });
        // Unsubscribe from old topics
        std::ranges::for_each(to_unsubscribe, [&](const auto& topic) { scribe(socket_pair, topic, false); });
        // Subscribe to new topics
        std::ranges::for_each(to_subscribe, [&](const auto& topic) { scribe(socket_pair, topic, true); });
    } else {
        // Subscribe to each topic not present in subscribed_topics_
        std::ranges::for_each(topics, [&](const auto& topic) {
            if(!subscribed_topics_.contains(topic)) {
                scribe(socket_pair, topic, true);
            }
        });
    }

    // Store new set of extra topics
    extra_subscribed_topics_[host] = std::move(topics);
}

void LogListener::subscribeExtra(const std::string& host, std::string topic) {
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

void LogListener::unsubscribeExtra(const std::string& host, const std::string& topic) {
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

void LogListener::removeExtraSubscriptions(const std::string& host) {
    // Lock sockets_mutex_ before to avoid potential deadlock
    const std::lock_guard sockets_lock {sockets_mutex_};
    auto& sockets = get_sockets();
    auto socket_pair = find_socket(host, sockets);

    const std::lock_guard subscribed_topics_lock {subscribed_topics_mutex_};
    const auto host_it = extra_subscribed_topics_.find(host);
    if(host_it != extra_subscribed_topics_.end()) {
        // Unscribe from each topic not in subscribed_topics_
        std::ranges::for_each(host_it->second, [&](const auto& topic) {
            if(!subscribed_topics_.contains(topic)) {
                scribe(socket_pair, topic, false);
            }
        });
        extra_subscribed_topics_.erase(host_it);
    }
}

void LogListener::removeExtraSubscriptions() {
    // Lock sockets_mutex_ before to avoid potential deadlock
    const std::lock_guard sockets_lock {sockets_mutex_};
    auto& sockets = get_sockets();

    const std::lock_guard subscribed_topics_lock {subscribed_topics_mutex_};
    std::ranges::for_each(extra_subscribed_topics_, [&](const auto& host_p) {
        auto socket_pair = find_socket(host_p.first, sockets);
        // Unscribe from each topic not in subscribed_topics_
        std::ranges::for_each(host_p.second, [&](const auto& topic) {
            if(!subscribed_topics_.contains(topic)) {
                scribe(socket_pair, topic, false);
            }
        });
    });
    extra_subscribed_topics_.clear();
}
