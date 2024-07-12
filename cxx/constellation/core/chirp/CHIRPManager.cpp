/**
 * @file
 * @brief Implementation of the CHIRP manager
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "CHIRPManager.hpp"

#include <chrono>
#include <functional>
#include <mutex>
#include <set>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

#include <asio.hpp>
#include <vector>

#include "constellation/core/chirp/CHIRPService.hpp"
#include "constellation/core/chirp/CHIRPSocket.hpp"
#include "constellation/core/logging/log.hpp"
#include "constellation/core/message/CHIRP2Message.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::chirp;
using namespace constellation::message;
using namespace constellation::utils;
using namespace std::chrono_literals;

using enum CHIRP2Message::Type;
using enum CHIRPService::Identifier;

CHIRPManager& CHIRPManager::getInstance() {
    static CHIRPManager instance {};
    return instance;
}

void CHIRPManager::initialize(std::string group_name, std::string host_name, asio::ip::address_v4 interface) {
    group_name_ = std::move(group_name);
    host_name_ = std::move(host_name);

    // Rebind to new correct interface
    socket_.setInterface(std::move(interface));

    // Register request callback
    registerRequestCallback(std::bind_front(&CHIRPManager::request_callback, this));

    // Start listening thread
    start_listening();
}

CHIRPManager::~CHIRPManager() {
    // First stop listening to stop callbacks
    stop_listening();
    // Now unregister all services
    unregisterServices();
}

void CHIRPManager::registerService(CHIRPService::Identifier service_identifier, Port port) {
    if(service_identifier == ANY) {
        // TODO throw
    }

    std::unique_lock registered_services_lock {registered_services_mutex_};
    const auto insert_ret = registered_services_.emplace(group_name_.value(), host_name_.value(), service_identifier, port);
    registered_services_lock.unlock();

    // If not inserted, throw
    if(!insert_ret.second) {
        // TODO throw
    }

    // Send offer for service
    send_message(OFFER, service_identifier, port);
}

void CHIRPManager::unregisterService(CHIRPService::Identifier service_identifier, Port port) {
    if(service_identifier == ANY) {
        // TODO throw
    }

    std::unique_lock registered_services_lock {registered_services_mutex_};
    const auto erase_ret = std::erase_if(registered_services_, [&](const auto& service) {
        return service.getServiceIdentifier() == service_identifier && service.getPort() == port;
    });
    registered_services_lock.unlock();

    // If not erased, throw
    if(erase_ret != 1) {
        // TODO throw
    }

    send_message(DEPART, service_identifier, port);
}

void CHIRPManager::unregisterServices() {
    const std::lock_guard registered_services_lock {registered_services_mutex_};
    for(const auto& service : registered_services_) {
        send_message(DEPART, service.getServiceIdentifier(), service.getPort());
    }
    registered_services_.clear();
}

/*std::set<RegisteredService> Manager::getRegisteredServices() {
    const std::lock_guard registered_services_lock {registered_services_mutex_};
    return registered_services_;
}*/

void CHIRPManager::sendRequest(CHIRPService::Identifier service_identifier) {
    send_message(REQUEST, service_identifier, 0);
}

void CHIRPManager::send_message(CHIRP2Message::Type type, CHIRPService::Identifier service_identifier, Port port) {
    LOG(logger_, DEBUG) << "Sending CHIRP " << to_string(type) << " for " << to_string(service_identifier)
                        << " service on port " << port;
    const auto msg = CHIRP2Message(group_name_.value(), host_name_.value(), type, service_identifier, port);
    socket_.send(msg.assemble().span());
}
