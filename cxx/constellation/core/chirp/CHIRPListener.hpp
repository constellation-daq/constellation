/**
 * @file
 * @brief CHIRP listener
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <asio/ip/address_v4.hpp>

#include "constellation/build.hpp"
#include "constellation/core/chirp/CHIRPService.hpp"
#include "constellation/core/chirp/CHIRPSocket.hpp"
#include "constellation/core/logging/Logger.hpp"

namespace constellation::chirp {

    class CHIRPListener {
    public:
        /** Callback type for a discover callback */
        enum class CallbackType : bool {
            /** Callback was called because a new service is offered */
            OFFER = true,
            /** Callback was called because an existing service departed */
            DEPART = false,
        };

    public:
        /**
         * @brief Construct a new CHIRP listener that listens to all CHIRP groups
         *
         * @param interface IP address of the network interface to use
         */
        CNSTLN_API CHIRPListener(asio::ip::address_v4 interface = asio::ip::address_v4::any());

        /**
         * @brief Construct a new CHIRP listener that listens to a specific CHIRP group
         *
         * @param group_name Name of the CHIRP group to listen to
         * @param interface IP address of the network interface to use
         */
        CNSTLN_API CHIRPListener(std::string group_name, asio::ip::address_v4 interface = asio::ip::address_v4::any());

        /**
         * @brief Construct a new CHIRP listener that listens to a specific CHIRP group and filters out a host
         *
         * @param group_name Name of the CHIRP group to listen to
         * @param group_name Host name which to filter out
         * @param interface IP address of the network interface to use
         */
        CNSTLN_API CHIRPListener(std::string group_name,
                                 std::string host_name,
                                 asio::ip::address_v4 interface = asio::ip::address_v4::any());

        CNSTLN_API virtual ~CHIRPListener();

        /**
         * @brief Get CHIRP group to which the listeners listens
         *
         * @return Optional with name of the CHIRP group, if set
         */
        std::optional<std::string_view> getGroupName() const { return group_name_; }

        /**
         * @brief Get host name which the listener filters out
         *
         * @return Optional with name of the host, if set
         */
        std::optional<std::string_view> getHostName() const { return host_name_; }

        /**
         * @brief Get all currently discovered service
         *
         * @param service_identifier Service identifier for which to return discovered services (default to any services)
         * @return List with pointers to discovered services
         */
        std::vector<std::shared_ptr<const CHIRPService>>
        getDiscoveredServices(CHIRPService::Identifier service_identifier = CHIRPService::ANY);

        /**
         * @brief Forget all discovered services
         */
        void forgetDiscoveredServices();

        /**
         * @brief Mark all service from a host as dead
         *
         * This removes all services that match the given host name from the discovered services list. If a service from the
         * host is discovered at a later point in time, it is simply added again.
         *
         * @param host_name Name of the host
         */
        void markDead(std::string_view host_name);

        /**
         * @brief Register a discover callback
         *
         * @param callback Callback function taking a pointer to service and if OFFER/DEPART as arguments
         */
        void registerDiscoverCallback(std::function<void(std::shared_ptr<const CHIRPService>, CallbackType)> callback);

        /**
         * @brief Register a request callback
         *
         * @param callback Callback function taking the service identifier and a reference to the chirp socket as arguments
         */
        void registerRequestCallback(std::function<void(CHIRPService::Identifier, CHIRPSocket&)> callback);

    protected:
        CHIRPListener(asio::ip::address_v4 interface,
                      std::optional<std::string> group_name,
                      std::optional<std::string> host_name,
                      bool start);

        void start_listening();
        void stop_listening();

    protected:
        CHIRPSocket socket_;

        std::optional<std::string> group_name_;
        std::optional<std::string> host_name_;

        log::Logger logger_;

    private:
        void handle_request(CHIRPService::Identifier service_identifier);

        void handle_offer_depart(CallbackType type, std::shared_ptr<CHIRPService> chirp_service);

        void listening_loop(const std::stop_token& stop_token);

    private:
        std::jthread listening_thread_;

        std::set<std::shared_ptr<CHIRPService>> discovered_services_;
        std::mutex discovered_services_mutex_;

        std::vector<std::function<void(std::shared_ptr<const CHIRPService>, CallbackType)>> discover_callbacks_;
        std::vector<std::function<void(CHIRPService::Identifier, CHIRPSocket&)>> request_callbacks_;
        std::mutex callbacks_mutex_;
    };

} // namespace constellation::chirp
