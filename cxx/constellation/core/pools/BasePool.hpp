/**
 * @file
 * @brief Abstract base pool
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <any>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <stop_token>
#include <thread>

#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/build.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/message/CHP1Message.hpp"
#include "constellation/core/message/exceptions.hpp"

namespace constellation::pools {

    /**
     * Abstract Base pool class
     *
     * This class registers a CHIRP callback for the services defined via the template parameter, listens to incoming
     * messages and forwards them to a callback registered upon creation of the socket
     */
    template <typename MESSAGE> class BasePool {
    public:
        /**
         * @brief Construct BasePool
         *
         * @param service CHIRP service identifier for which a subscription should be made
         * @param log_topic Logger topic to be used for this component
         * @param callback Callback function pointer for received messages
         * @param socket_type ZMQ socket type to use for connecting to service, e.g. sub or pull
         */
        BasePool(chirp::ServiceIdentifier service,
                 std::string_view log_topic,
                 std::function<void(const MESSAGE&)> callback,
                 zmq::socket_type socket_type);

        /**
         * @brief Destruct BasePool
         *
         * This closes all connections and unregisters the CHIRP service discovery callback
         */
        virtual ~BasePool();

        /// @cond doxygen_suppress
        BasePool(const BasePool& other) = delete;
        BasePool& operator=(const BasePool& other) = delete;
        BasePool(BasePool&& other) = delete;
        BasePool& operator=(BasePool&& other) = delete;
        /// @endcond

        /**
         * @brief Check if pool thread has thrown an exception
         * @throw Exception thrown by pool thread, if any
         */
        void checkException();

    protected:
        /**
         * @brief Start the pool thread and send the CHIRP requests
         */
        void start();

        /**
         * @brief Method to select which services to connect to. By default this pool connects to all discovered services,
         * derived pools may implement selection criteria
         *
         * @param service The discovered service
         * @return Boolean indicating whether a connection should be opened
         */
        virtual bool should_connect(const chirp::DiscoveredService& service);

        /**
         * @brief Method for derived classes to act on newly connected sockets
         *
         * @param socket The newly connected socket
         */
        virtual void socket_connected(zmq::socket_t& socket);

        /**
         * @brief Method for derived classes to act on sockets before disconnecting
         *
         * @param socket The socket to be disconnected
         */
        virtual void socket_disconnected(zmq::socket_t& socket);

    private:
        /**
         * @brief Callback for CHIRP service discovery
         * @details This callback is run for every service of the given type discovered on or departing from the
         * constellation. It will connect or disconnect the remote service and register the corresponding socket
         * with the central polling mechanism
         *
         * @param service Discovered service
         * @param depart Boolean to indicate discovery or departure
         * @param user_data Pointer to the BasePool instance
         */
        static void callback(chirp::DiscoveredService service, bool depart, std::any user_data);

        /** Callback implementation */
        void callback_impl(const chirp::DiscoveredService& service, bool depart);

        /** Main loop polling the registered sockets */
        void loop(const std::stop_token& stop_token);

        /** Helper to connect to a newly discovered socket */
        void connect(const chirp::DiscoveredService& service);

        /** Helper to disconnect from a departing service */
        void disconnect(const chirp::DiscoveredService& service);

        /** Disconnect from all registered services */
        void disconnect_all();

    protected:
        std::map<chirp::DiscoveredService, zmq::socket_t> sockets_; // NOLINT(*-non-private-member-variables-in-classes)
        std::timed_mutex sockets_mutex_;                            // NOLINT(*-non-private-member-variables-in-classes)

        log::Logger logger_; // NOLINT(*-non-private-member-variables-in-classes)

    private:
        chirp::ServiceIdentifier service_;

        zmq::active_poller_t poller_;

        std::atomic_flag af_;
        std::jthread pool_thread_;
        std::exception_ptr exception_ptr_ {nullptr};

        std::function<void(const MESSAGE&)> message_callback_;
        zmq::socket_type socket_type_;
    };
} // namespace constellation::pools

// Include template members
#include "BasePool.ipp"
