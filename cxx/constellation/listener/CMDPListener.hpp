/**
 * @file
 * @brief Subscriber pool for CMDP
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

#include "constellation/build.hpp"
#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/pools/SubscriberPool.hpp"
#include "constellation/core/utils/string_hash_map.hpp"

namespace constellation::listener {

    class CNSTLN_API CMDPListener
        : public pools::SubscriberPool<message::CMDP1Message, chirp::ServiceIdentifier::MONITORING> {
    public:
        using SubscriberPoolT = pools::SubscriberPool<message::CMDP1Message, chirp::ServiceIdentifier::MONITORING>;

        /**
         * @brief Construct CMDPListener
         *
         * @param log_topic Logger topic to be used for this component
         * @param callback Callback function pointer for received messages
         */
        CNSTLN_API CMDPListener(std::string_view log_topic, std::function<void(message::CMDP1Message&&)> callback);

        // No copy/move constructor/assignment
        /// @cond doxygen_suppress
        CMDPListener(const CMDPListener& other) = delete;
        CMDPListener& operator=(const CMDPListener& other) = delete;
        CMDPListener(CMDPListener&& other) noexcept = delete;
        CMDPListener& operator=(CMDPListener&& other) = delete;
        /// @endcond

        CNSTLN_API virtual ~CMDPListener() = default;

        /**
         * @brief Method to set the topics this pool subscribe to for all sockets
         *
         * @warning This replaces all previously subscribed topics
         *
         * @param topics Set of subscription topics to which to subscribe to
         */
        CNSTLN_API void setTopicSubscriptions(std::set<std::string> topics);

        /**
         * @brief Subscribe to a given topic for all sockets
         *
         * @param topic Topic to subscribe to
         */
        CNSTLN_API void subscribeTopic(std::string topic);

        /**
         * @brief Unsubscribe from a given topic for all sockets
         *
         * @param topic Topic to unsubscribe
         */
        CNSTLN_API void unsubscribeTopic(const std::string& topic);

        /**
         * @brief Get set of subscribed topics for all sockets
         *
         * @return Set containing the currently subscribed topics
         */
        CNSTLN_API std::set<std::string> getTopicSubscriptions();

        /**
         * @brief Method to set the extra topics this pool subscribe to for a specific socket
         *
         * @note Extra topics are topics subscribed to in addition to the topics for every socket
         * @warning This replaces all previously subscribed extra topics
         *
         * @param host Canonical name of the host to set subscription topics
         * @param topics Set of subscription topics to which to subscribe all sockets
         */
        CNSTLN_API void setExtraTopicSubscriptions(const std::string& host, std::set<std::string> topics);

        /**
         * @brief Subscribe to a given topic for a specific socket
         *
         * @note Extra topics are topics subscribed to in addition to the topics for every socket
         *
         * @param host Canonical name of the host to subscribe to
         * @param topic Topic to subscribe to
         */
        CNSTLN_API void subscribeExtraTopic(const std::string& host, std::string topic);

        /**
         * @brief Unsubscribe from a given topic for a specific socket
         *
         * @note Only unsubscribes if not in topics that every socket is subscribed to
         *
         * @param host Canonical name of the host to unsubscribe from
         * @param topic Topic to unsubscribe
         */
        CNSTLN_API void unsubscribeExtraTopic(const std::string& host, const std::string& topic);

        /**
         * @brief Get set of subscribed extra topics for a specific socket
         *
         * @note Extra topics are topics subscribed to in addition to the topics for every socket
         *
         * @return Set containing the currently subscribed extra topics for given host
         */
        CNSTLN_API std::set<std::string> getExtraTopicSubscriptions(const std::string& host);

        /**
         * @brief Remove extra topics for a specific socket
         *
         * @param host Canonical name of the host
         */
        CNSTLN_API void removeExtraTopicSubscriptions(const std::string& host);

        /**
         * @brief Remove extra topics for all sockets
         */
        CNSTLN_API void removeExtraTopicSubscriptions();

    protected:
        /**
         * @brief Method for derived classes to act on newly connected sockets
         *
         * @warning Derived functions should always call `CMDPPool::socket_connected()` to ensure that sockets are
         *          subscribed to the correct topics.
         */
        CNSTLN_API void host_connected(const chirp::DiscoveredService& service) override;

    private:
        // Hide subscribe/unsubscribe functions from SubscriberPool
        using SubscriberPoolT::subscribe;
        using SubscriberPoolT::unsubscribe;

    private:
        std::mutex subscribed_topics_mutex_;
        std::set<std::string> subscribed_topics_;
        utils::string_hash_map<std::set<std::string>> extra_subscribed_topics_;
    };

} // namespace constellation::listener
