/**
 * @file
 * @brief Abstract subscriber pool
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <string_view>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/pools/BasePool.hpp"
#include "constellation/core/utils/string_hash_map.hpp"

#include "zmq.hpp"

namespace constellation::pools {

    /**
     * Abstract Subscriber pool class
     *
     * This class registers a CHIRP callback for the services defined via the template parameter, listens to incoming
     * messages and forwards them to a callback registered upon creation of the subscriber socket
     */
    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    class SubscriberPool : public BasePool<MESSAGE, SERVICE, zmq::socket_type::sub> {
    public:
        using BasePoolT = BasePool<MESSAGE, SERVICE, zmq::socket_type::sub>;

        /**
         * @brief Construct SubscriberPool
         *
         * @param log_topic Logger topic to be used for this component
         * @param callback Callback function pointer for received messages
         */
        SubscriberPool(std::string_view log_topic, std::function<void(MESSAGE&&)> callback);

        /*
         * @brief Method to update the topics this pool subscribe to for all sockets
         *
         * @param topics Set of subscription topics to which to subscribe to
         */
        void setSubscriptionTopics(std::set<std::string> topics);

        /**
         * @brief Subscribe to a given topic for all sockets
         *
         * @param topic Topic to subscribe to
         */
        void subscribe(std::string topic);

        /**
         * @brief Unsubscribe from a given topic for all sockets
         *
         * @param topic Topic to unsubscribe
         */
        void unsubscribe(const std::string& topic);

        /*
         * @brief Method to update the extra topics this pool subscribe to for a specific socket
         *
         * @note Extra topics are topics subscribed to in addition to the topics for every socket
         *
         * @param host Canonical name of the host to set subscription topics
         * @param topics Set of subscription topics to which to subscribe all sockets
         */
        void setExtraSubscriptionTopics(const std::string& host, std::set<std::string> topics);

        /**
         * @brief Subscribe to a given topic for a specific socket
         *
         * @param host Canonical name of the host to subscribe to
         * @param topic Topic to subscribe to
         */
        void subscribeExtra(const std::string& host, std::string topic);

        /**
         * @brief Unsubscribe from a given topic for a specific socket
         *
         * @note Only unsubscribes if not in topics that every socket is subscribed to
         *
         * @param host Canonical name of the host to unsubscribe from
         * @param topic Topic to unsubscribe
         */
        void unsubscribeExtra(const std::string& host, const std::string& topic);

        /**
         * @brief Remove extra topics for a specific socket
         *
         * @param host Canonical name of the host
         */
        void removeExtraSubscriptions(const std::string& host);

        /**
         * @brief Remove extra topics for all sockets
         */
        void removeExtraSubscriptions();

    private:
        void socket_connected(const chirp::DiscoveredService& service, zmq::socket_t& socket) final;

        /** Sub- or unsubscribe to a topic for a single host */
        void scribe(std::string_view host, std::string_view topic, bool subscribe);

        /** Sub- or unsubscribe to a topic for all connected hosts */
        void scribe_all(std::string_view topic, bool subscribe);

        std::mutex subscribed_topics_mutex_;
        std::set<std::string> subscribed_topics_;
        utils::string_hash_map<std::set<std::string>> extra_subscribed_topics_;
    };
} // namespace constellation::pools

// Include template members
#include "SubscriberPool.ipp" // IWYU pragma: keep
