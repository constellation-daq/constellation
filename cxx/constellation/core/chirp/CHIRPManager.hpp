/**
 * @file
 * @brief CHIRP Manager
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <memory>
#include <mutex>
#include <set>
#include <string>

#include <asio/ip/address_v4.hpp>

#include "constellation/build.hpp"
#include "constellation/core/chirp/CHIRPListener.hpp"
#include "constellation/core/chirp/CHIRPService.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/core/message/CHIRP2Message.hpp"
#include "constellation/core/utils/networking.hpp"

namespace constellation::chirp {

    /** Manager for registering CHIRP services */
    class CHIRPManager : public CHIRPListener {
    public:
        CNSTLN_API static CHIRPManager& getInstance();

        // No copy/move constructor/assignment
        /// @cond doxygen_suppress
        CHIRPManager(const CHIRPManager& other) = delete;
        CHIRPManager& operator=(const CHIRPManager& other) = delete;
        CHIRPManager(CHIRPManager&& other) = delete;
        CHIRPManager& operator=(CHIRPManager&& other) = delete;
        /// @endcond

        CNSTLN_API virtual ~CHIRPManager();

        /**
         * @brief Initialize the manager
         *
         * Creates the `CHIRPListener` with given group name, host name and network interface.
         *
         * @param group_name Name of the CHIRP group
         * @param host_name Name of the host
         * @param interface IP address of the network interface to use
         */
        CNSTLN_API void initialize(std::string group_name,
                                   std::string host_name,
                                   asio::ip::address_v4 interface = asio::ip::address_v4::any());

        /**
         * @brief Register a service offered by the host in the manager
         *
         * Calling this function sends a CHIRP message with OFFER type, and registers the service such that the manager
         * responds to CHIRP messages with REQUEST type and the corresponding service identifier.
         *
         * @param service_identifier Service identifier of the offered service
         * @param port Port of the offered service
         */
        CNSTLN_API void registerService(CHIRPService::Identifier service_identifier, utils::Port port);

        /**
         * @brief Unregister a previously registered service offered by the host in the manager
         *
         * Calling this function sends a CHIRP message with DEPART type and removes the service from manager.
         *
         * @param service_identifier Service identifier of the previously offered service
         * @param port Port of the previously offered service
         */
        CNSTLN_API void unregisterService(CHIRPService::Identifier service_identifier, utils::Port port);

        /**
         * @brief Unregisters all offered services registered in the manager
         *
         * Equivalent to calling `unregisterService()` for every registered service.
         */
        CNSTLN_API void unregisterServices();

        /**
         * @brief Get all currently in the manager registered services
         *
         * @return Set with all currently registered services
         */
        CNSTLN_API std::set<CHIRPService> getRegisteredServices();

        /**
         * @brief Send a discovery request for a specific service identifier
         *
         * This sends a CHIRP message with a REQUEST type and a given service identifier. Other hosts might reply with a
         * CHIRP message with OFFER type for the given service identifier. These can be retrieved either by registering a
         * user callback (see `CHIRPListener::registerDiscoverCallback()`) or by getting the list of discovered services
         * shortly after (see `CHIRPListener::getDiscoveredServices()`).
         *
         * @param service_identifier Service identifier to send a request for
         */
        CNSTLN_API void sendRequest(CHIRPService::Identifier service_identifier);

    private:
        CHIRPManager();

        void request_callback(CHIRPService::Identifier service_identifier, CHIRPSocket& socket);

        void send_message(message::CHIRP2Message::Type type, CHIRPService::Identifier service_identifier, utils::Port port);

    private:
        std::set<CHIRPService> registered_services_;
        std::mutex registered_services_mutex_;
    };

} // namespace constellation::chirp
