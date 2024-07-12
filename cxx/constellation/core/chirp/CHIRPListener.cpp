/**
 * @file
 * @brief Implementation of CHIRP listener
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "CHIRPListener.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <asio.hpp>

#include "constellation/core/chirp/CHIRPService.hpp"
#include "constellation/core/chirp/CHIRPSocket.hpp"
#include "constellation/core/logging/log.hpp"
#include "constellation/core/message/CHIRP2Message.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/utils/networking.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::chirp;
using namespace constellation::message;
using namespace constellation::utils;
using namespace std::chrono_literals;

CHIRPListener::CHIRPListener(asio::ip::address_v4 interface,
                             std::optional<std::string> group_name,
                             std::optional<std::string> host_name,
                             bool start)
    : socket_(std::move(interface)), group_name_(std::move(group_name)), host_name_(std::move(host_name)),
      logger_("CHIRP" + (group_name_.has_value() ? "_" + group_name_.value() : "")) {
    // Start listening thread
    if(start) {
        start_listening();
    }
}

CHIRPListener::CHIRPListener(asio::ip::address_v4 interface) : CHIRPListener(std::move(interface), {}, {}, true) {}

CHIRPListener::CHIRPListener(std::string group_name, asio::ip::address_v4 interface)
    : CHIRPListener(std::move(interface), std::move(group_name), {}, true) {}

CHIRPListener::CHIRPListener(std::string group_name, std::string host_name, asio::ip::address_v4 interface)
    : CHIRPListener(std::move(interface), std::move(group_name), std::move(host_name), true) {}

CHIRPListener::~CHIRPListener() {
    stop_listening();
}

void CHIRPListener::start_listening() {
    listening_thread_ = std::jthread(std::bind_front(&CHIRPListener::listening_loop, this));
}

void CHIRPListener::stop_listening() {
    listening_thread_.request_stop();
    if(listening_thread_.joinable()) {
        listening_thread_.join();
    }
}

std::vector<std::shared_ptr<const CHIRPService>>
CHIRPListener::getDiscoveredServices(CHIRPService::Identifier service_identifier) {
    std::vector<std::shared_ptr<const CHIRPService>> ret {};
    const std::lock_guard discovered_services_lock {discovered_services_mutex_};

    for(const auto& discovered_service : discovered_services_) {
        if(discovered_service->getServiceIdentifier() == service_identifier || service_identifier == CHIRPService::ANY) {
            ret.emplace_back(discovered_service);
        }
    }

    return ret;
}

void CHIRPListener::forgetDiscoveredServices() {
    const std::lock_guard discovered_services_lock {discovered_services_mutex_};
    discovered_services_.clear();
    LOG(logger_, TRACE) << "Dropped all discovered services";
}

void CHIRPListener::markDead(std::string_view host_name) {
    const std::lock_guard discovered_services_lock {discovered_services_mutex_};
    // Erase every service where the host name matches
    const auto dropped = std::erase_if(discovered_services_,
                                       [&host_name](const auto& service) { return service->getHostName() == host_name; });
    LOG(logger_, TRACE) << "Dropped " << dropped << (dropped == 1 ? "service" : "services") << " for host " << host_name;
}

void CHIRPListener::registerDiscoverCallback(
    std::function<void(std::shared_ptr<const CHIRPService>, CallbackType)> callback) {
    // Acquire lock for callback vectors
    const std::lock_guard callbacks_lock {callbacks_mutex_};
    // Add to discover callbacks
    discover_callbacks_.emplace_back(std::move(callback));
}

void CHIRPListener::registerRequestCallback(std::function<void(CHIRPService::Identifier, CHIRPSocket&)> callback) {
    // Acquire lock for callback vectors
    const std::lock_guard callbacks_lock {callbacks_mutex_};
    // Add to request callbacks
    request_callbacks_.emplace_back(std::move(callback));
}

void CHIRPListener::handle_request(CHIRPService::Identifier service_identifier) {
    LOG(logger_, TRACE) << "Received REQUEST for " << to_string(service_identifier);
    // Acquire lock for callback vectors
    const std::lock_guard callbacks_lock {callbacks_mutex_};
    // Loop over request callback and run as detached threads
    for(const auto& callback : request_callbacks_) {
        std::thread(callback, service_identifier, std::ref(socket_)).detach();
    }
}

void CHIRPListener::handle_offer_depart(CallbackType type, std::shared_ptr<CHIRPService> chirp_service) {
    // Acquire lock for thread safe access to discovered_services_lock
    std::unique_lock discovered_services_lock {discovered_services_mutex_};

    // Add/remove from discovered services
    if(type == CallbackType::OFFER) {
        // Return if service already discovered
        if(discovered_services_.contains(chirp_service)) {
            return;
        }
        discovered_services_.insert(chirp_service);
        LOG(logger_, DEBUG) << to_string(chirp_service->getServiceIdentifier()) << " service at " << chirp_service->getURI()
                            << " discovered";
    } else {
        // Return if service not previously discovered
        if(!discovered_services_.contains(chirp_service)) {
            return;
        }
        discovered_services_.erase(chirp_service);
        LOG(logger_, DEBUG) << to_string(chirp_service->getServiceIdentifier()) << " service at " << chirp_service->getURI()
                            << " departed";
    }

    // Unlock lock to allow access to list of all services in callback
    discovered_services_lock.unlock();
    // Acquire lock for callback vectors
    const std::lock_guard callbacks_lock {callbacks_mutex_};
    // Loop over discover callback and run as detached threads
    for(const auto& callback : discover_callbacks_) {
        std::thread(callback, std::move(chirp_service), type).detach();
    }
}

void CHIRPListener::listening_loop(const std::stop_token& stop_token) {
    LOG(logger_, INFO) << "Starting to listen to CHIRP" << (group_name_.has_value() ? " group " + group_name_.value() : "")
                       << (host_name_.has_value() ? " for host " + host_name_.value() : "") << " on interface "
                       << address_to_ip(socket_.getInterface());

    while(!stop_token.stop_requested()) {
        const auto recv_res_opt = socket_.recv(50ms);

        // Check for timeout
        if(!recv_res_opt.has_value()) {
            continue;
        }

        const auto& [message_buffer, sender_endpoint] = recv_res_opt.value();
        LOG(logger_, TRACE) << "Received message from "
                            << endpoint_to_uri("udp", sender_endpoint.address().to_v4(), sender_endpoint.port());

        try {
            const auto chirp_msg = CHIRP2Message::disassemble(message_buffer);

            // Check that from same group
            if(group_name_.has_value() && chirp_msg.getGroupName() != group_name_.value()) {
                LOG(logger_, TRACE) << "Ignoring CHIRP message from CHIRP group \"" << chirp_msg.getGroupName() << "\"";
                continue;
            }

            // Check if from same host
            if(host_name_.has_value() && chirp_msg.getHostName() != host_name_.value()) {
                LOG(logger_, TRACE) << "Ignoring CHIRP message from host \"" << chirp_msg.getHostName() << "\"";
                continue;
            }

            using enum CHIRP2Message::Type;
            const auto type = chirp_msg.getType();

            // Handle for REQUEST
            if(type == REQUEST) {
                handle_request(chirp_msg.getServiceIdentifier());
                continue;
            }

            // Create CHIRP service if OFFER/DEPART
            const auto chirp_service = std::make_shared<CHIRPService>(to_string(chirp_msg.getGroupName()),
                                                                      to_string(chirp_msg.getHostName()),
                                                                      chirp_msg.getServiceIdentifier(),
                                                                      chirp_msg.getPort(),
                                                                      sender_endpoint.address().to_v4());
            LOG(logger_, TRACE) << "Received " << to_string(type) << " for CHIRP service "
                                << ": group = " << chirp_service->getGroupName()
                                << ", host = " << chirp_service->getHostName()
                                << ", service = " << to_string(chirp_service->getServiceIdentifier())
                                << ", port = " << chirp_service->getPort()
                                << ", address = " << address_to_ip(chirp_service->getAddress());

            // Handle for OFFER/DEPART
            handle_offer_depart(static_cast<CallbackType>(type == OFFER), chirp_service);

        } catch(const MessageDecodingError& error) {
            LOG(logger_, WARNING) << "Failed to decode CHIRP message: " << error.what();
        }
    }
}
