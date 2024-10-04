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
#include <initializer_list>
#include <set>
#include <string>
#include <string_view>

#include <zmq.hpp>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/pools/BasePool.hpp"

namespace constellation::pools {

    /**
     * Abstract Subscriber pool class
     *
     * This class registers a CHIRP callback for the services defined via the template parameter, listens to incoming
     * messages and forwards them to a callback registered upon creation of the subscriber socket.
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
         * @param default_topics List of default subscription topics to which this component subscribes directly upon
         *                       opening the socket
         */
        SubscriberPool(std::string_view log_topic,
                       std::function<void(MESSAGE&&)> callback,
                       std::initializer_list<std::string> default_topics = {});

    protected:
        /**
         * @brief Method for derived classes to act on newly connected sockets
         *
         * @warning Derived functions should always call `SubscriberPool::socket_connected()` to ensure that sockets are
         *          subscribed to the default topics.
         */
        void socket_connected(const chirp::DiscoveredService& service, zmq::socket_t& socket) override;

    private:
        std::set<std::string> default_topics_;
    };
} // namespace constellation::pools

// Include template members
#include "SubscriberPool.ipp" // IWYU pragma: keep
