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
#include <string>
#include <string_view>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/pools/BasePool.hpp"

#include "zmq.hpp"

namespace constellation::pools {

    /**
     * Abstract Subscriber pool class
     *
     * This class registers a CHIRP callback for the services defined via the template parameter, listens to incoming
     * messages and forwards them to a callback registered upon creation of the subscriber socket.
     *
     * @warning Duplicate subscriptions also require duplicate unsubscriptions, this class does not contain any logic to
     *          to track subscription states.
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

        /**
         * @brief Subscribe to a given topic of a specific host
         *
         * @param host Canonical name of the host to subscribe to
         * @param topic Topic to subscribe to
         */
        void subscribe(std::string_view host, const std::string& topic);

        /**
         * @brief Subscribe to a given topic of a specific host
         *
         * @param host_id MD5 hash of canonical name of the host to subscribe to
         * @param topic Topic to subscribe to
         */
        void subscribe(message::MD5Hash host_id, const std::string& topic);

        /**
         * @brief Subscribe to a given topic for all connected hosts
         *
         * @param topic Topic to subscribe to
         */
        void subscribe(const std::string& topic);

        /**
         * @brief Unsubscribe from a given topic of a specific host
         *
         * @param host Canonical name of the host to unsubscribe from
         * @param topic Topic to unsubscribe
         */
        void unsubscribe(std::string_view host, const std::string& topic);

        /**
         * @brief Unsubscribe from a given topic of a specific host
         *
         * @param host_id MD5 hash of canonical name of the host to unsubscribe from
         * @param topic Topic to unsubscribe
         */
        void unsubscribe(message::MD5Hash host_id, const std::string& topic);

        /**
         * @brief Unsubscribe from a given topic for all hosts
         *
         * @param topic Topic to unsubscribe
         */
        void unsubscribe(const std::string& topic);

    private:
        /** Sub- or unsubscribe to a topic for a single host */
        void scribe(message::MD5Hash host_id, const std::string& topic, bool subscribe);

        /** Sub- or unsubscribe to a topic for all connected hosts */
        void scribe_all(const std::string& topic, bool subscribe);
    };
} // namespace constellation::pools

// Include template members
#include "SubscriberPool.ipp" // IWYU pragma: keep
