/**
 * @file
 * @brief CHIRP Manager
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <any>
#include <cstdint>
#include <mutex>
#include <set>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <asio.hpp>

#include "constellation/build.hpp"
#include "constellation/core/chirp/BroadcastRecv.hpp"
#include "constellation/core/chirp/BroadcastSend.hpp"
#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/utils/networking.hpp"

namespace constellation::chirp {

    /** A service offered by the host and announced by the `Manager` */
    struct RegisteredService {
        /** Service identifier of the offered service */
        ServiceIdentifier identifier;

        /** Port of the offered service */
        utils::Port port;

        CNSTLN_API bool operator<(const RegisteredService& other) const;
    };

    /** A service discovered by the `Manager` */
    struct DiscoveredService {
        /** Address of the discovered service */
        asio::ip::address_v4 address;

        /** Host ID of the discovered service */
        message::MD5Hash host_id;

        /** Service identifier of the discovered service */
        ServiceIdentifier identifier;

        /** Port of the discovered service */
        utils::Port port;

        /** Convert service information to a URI */
        CNSTLN_API std::string to_uri() const;

        CNSTLN_API bool operator<(const DiscoveredService& other) const;
    };

    /** Status of a service for callbacks from the `Manager` */
    enum class ServiceStatus : std::uint8_t {
        /** The service is newly discovered */
        DISCOVERED,
        /** The service departed */
        DEPARTED,
        /** The service is considered dead without departure */
        DEAD,
    };

    /**
     * Function signature for user callback
     *
     * The first argument (``service``) contains the discovered service, the second argument is an enum that indicates the
     * status of the service, i.e. whether it is newly discovered, departing, or considered dead, and the third argument
     * (``user_data``) is arbitrary user data passed to the callback (done via `Manager::RegisterDiscoverCallback`).
     *
     * It is recommended to pass the user data wrapped in an atomic `std::shared_ptr` since the callback is
     * launched asynchronously in a detached `std::thread`. If the data is modified, it is recommended to use
     * atomic types when possible or a `std::mutex` for locking to ensure thread-safe access.
     */
    using DiscoverCallback = void(DiscoveredService service, ServiceStatus status, std::any user_data);

    /** Entry for a user callback in the `Manager` for newly discovered or departing services */
    struct DiscoverCallbackEntry {
        /** Function pointer to a callback */
        DiscoverCallback* callback;

        /** Service identifier of the service for which callbacks should be received */
        ServiceIdentifier service_id;

        /**
         * Arbitrary user data passed to the callback function
         *
         * For information on lifetime and thread-safety see `DiscoverCallback`.
         */
        std::any user_data;

        CNSTLN_API bool operator<(const DiscoverCallbackEntry& other) const;
    };

    /** Manager for CHIRP broadcasting and receiving */
    class Manager {
    public:
        /**
         * Return the default CHIRP Manager (requires to be set via `setAsDefaultInstance`)
         *
         * @return Pointer to default CHIRP Manager (might be a nullptr)
         */
        CNSTLN_API static Manager* getDefaultInstance();

        /**
         * Set this CHIRP manager as the default instance
         */
        CNSTLN_API void setAsDefaultInstance();

    public:
        /**
         * @param brd_address Broadcast address for outgoing broadcast messages
         * @param any_address Any address for incoming broadcast messages
         * @param group_name Group name of the group to join
         * @param host_name Host name for outgoing messages
         */
        CNSTLN_API Manager(const asio::ip::address_v4& brd_address,
                           const asio::ip::address_v4& any_address,
                           std::string_view group_name,
                           std::string_view host_name);

        /**
         * @param brd_ip Broadcast IP for outgoing broadcast messages
         * @param any_ip Any IP for incoming broadcast messages
         * @param group_name Group name of the group to join
         * @param host_name Host name for outgoing messages
         */
        CNSTLN_API
        Manager(std::string_view brd_ip, std::string_view any_ip, std::string_view group_name, std::string_view host_name);

        CNSTLN_API virtual ~Manager();

        // No copy/move constructor/assignment
        /// @cond doxygen_suppress
        Manager(const Manager& other) = delete;
        Manager& operator=(const Manager& other) = delete;
        Manager(Manager&& other) = delete;
        Manager& operator=(Manager&& other) = delete;
        /// @endcond

        /**
         * @return Group ID (MD5Hash of the group name)
         */
        constexpr message::MD5Hash getGroupID() const { return group_id_; }

        /**
         * @return Host ID (MD5Hash of the host name)
         */
        constexpr message::MD5Hash getHostID() const { return host_id_; }

        /** Start the background thread of the manager */
        CNSTLN_API void start();

        /**
         * Register a service offered by the host in the manager
         *
         * Calling this function sends a CHIRP broadcast with OFFER type, and registers the service such that the manager
         * responds to CHIRP broadcasts with REQUEST type and the corresponding service identifier.
         *
         * @param service_id Service identifier of the offered service
         * @param port Port of the offered service
         * @retval true if the service was registered
         * @retval false if the service was already registered
         */
        CNSTLN_API bool registerService(ServiceIdentifier service_id, utils::Port port);

        /**
         * Unregister a previously registered service offered by the host in the manager
         *
         * Calling this function sends a CHIRP broadcast with DEPART type and removes the service from manager. See also
         * `RegisterService`.
         *
         * @param service_id Service identifier of the previously offered service
         * @param port Port of the previously offered service
         * @retval true If the service was unregistered
         * @retval false If the service was never registered
         */
        CNSTLN_API bool unregisterService(ServiceIdentifier service_id, utils::Port port);

        /**
         * Unregisters all offered services registered in the manager
         *
         * Equivalent to calling `UnregisterService` for every registered service.
         */
        CNSTLN_API void unregisterServices();

        /**
         * Get the list of services currently registered in the manager
         *
         * @return Set with all currently registered services
         */
        CNSTLN_API std::set<RegisteredService> getRegisteredServices();

        /**
         * Register a user callback for newly discovered or departing services
         *
         * Note that a callback function can be registered multiple times for different services.
         *
         * @warning Discover callbacks block the execution of further processing of CHIRP requests and offers, callbacks that
         *          take a long time should offload the work to a new thread.
         *
         * @param callback Function pointer to a callback
         * @param service_id Service identifier of the service for which callbacks should be received
         * @param user_data Arbitrary user data passed to the callback function (see `DiscoverCallback`)
         * @retval true If the callback/service/user_data combination was registered
         * @retval false If the callback/service/user_data combination was already registered
         */
        CNSTLN_API bool registerDiscoverCallback(DiscoverCallback* callback,
                                                 ServiceIdentifier service_id,
                                                 std::any user_data);

        /**
         * Unegister a previously registered callback for newly discovered or departing services
         *
         * @param callback Function pointer to the callback of registered callback entry
         * @param service_id Service identifier of registered callback entry
         * @retval true If the callback entry was unregistered
         * @retval false If the callback entry was never registered
         */
        CNSTLN_API bool unregisterDiscoverCallback(DiscoverCallback* callback, ServiceIdentifier service_id);

        /**
         * Unregisters all discovery callbacks registered in the manager
         *
         * Equivalent to calling `UnregisterDiscoverCallback` for every discovery callback.
         */
        CNSTLN_API void unregisterDiscoverCallbacks();

        /**
         * @brief Forget the previously discovered services of the given type and host ID, if present
         *
         * @param identifier Identifier of the discovered service to be forgotten
         * @param host_id Host ID of the discovered service to be forgotten
         */
        CNSTLN_API void forgetDiscoveredService(ServiceIdentifier identifier, message::MD5Hash host_id);

        /**
         * @brief Forget all previously discovered services of a given host
         *
         * @param host_id Host ID of the discovered services to be forgotten
         */
        CNSTLN_API void forgetDiscoveredServices(message::MD5Hash host_id);

        /** Forget all previously discovered services */
        CNSTLN_API void forgetDiscoveredServices();

        /**
         * Returns list of all discovered services
         *
         * @return Vector with all discovered services
         */
        CNSTLN_API std::vector<DiscoveredService> getDiscoveredServices();

        /**
         * Returns list of all discovered services with a given service identifier
         *
         * @param service_id Service identifier for discovered services that should be listed
         * @return Vector with all discovered services with the given service identifier
         */
        CNSTLN_API std::vector<DiscoveredService> getDiscoveredServices(ServiceIdentifier service_id);

        /**
         * Send a discovery request for a specific service identifier
         *
         * This sends a CHIRP broadcast with a REQUEST type and a given service identifier. Other hosts might reply with a
         * CHIRP broadcast with OFFER type for the given service identifier. These can be retrieved either by registering a
         * user callback (see `RegisterDiscoverCallback`) or by getting the list of discovered services shortly
         * after (see `GetDiscoveredServices`).
         *
         * @param service_id Service identifier to send a request for
         */
        CNSTLN_API void sendRequest(ServiceIdentifier service_id);

    private:
        /**
         * Send a CHIRP broadcast
         *
         * @param type CHIRP broadcast message type
         * @param service Service with identifier and port
         */
        void send_message(MessageType type, RegisteredService service);

        /**
         * Call all discover callbacks
         */
        void call_discover_callbacks(const DiscoveredService& discovered_service, ServiceStatus status);

        /**
         * Main loop listening and responding to incoming CHIRP broadcasts
         *
         * The run loop responds to incoming CHIRP broadcasts with REQUEST type by sending CHIRP broadcasts with OFFER type
         * for all registered services. It also tracks incoming CHIRP broadcasts with OFFER and DEPART type to form the list
         * of discovered services and calls the corresponding discovery callbacks.
         *
         * @param stop_token Token to stop loop via `std::jthread`
         */
        void main_loop(const std::stop_token& stop_token);

    private:
        BroadcastRecv receiver_;
        BroadcastSend sender_;

        message::MD5Hash group_id_;
        message::MD5Hash host_id_;

        log::Logger logger_;

        /** Set of registered services */
        std::set<RegisteredService> registered_services_;

        /** Mutex for thread-safe access to `registered_services_` */
        std::mutex registered_services_mutex_;

        /** Set of discovered services */
        std::set<DiscoveredService> discovered_services_;

        /** Mutex for thread-safe access to `discovered_services_` */
        std::mutex discovered_services_mutex_;

        /** Set of discovery callbacks */
        std::set<DiscoverCallbackEntry> discover_callbacks_;

        /** Mutex for thread-safe access to `discover_callbacks_` */
        std::mutex discover_callbacks_mutex_;

        std::jthread main_loop_thread_;

        // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
        inline static Manager* default_manager_instance_;
    };

} // namespace constellation::chirp
