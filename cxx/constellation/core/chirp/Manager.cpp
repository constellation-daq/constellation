/**
 * @file
 * @brief Implementation of the CHIRP manager
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Manager.hpp"

#include <algorithm>
#include <any>
#include <chrono>
#include <compare>
#include <cstdint>
#include <functional>
#include <future>
#include <iterator>
#include <mutex>
#include <set>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/utils/networking.hpp"
#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::chirp;
using namespace constellation::message;
using namespace constellation::utils;
using namespace std::chrono_literals;

bool RegisteredService::operator<(const RegisteredService& other) const {
    // Sort first by service id
    auto ord_id = std::to_underlying(identifier) <=> std::to_underlying(other.identifier);
    if(std::is_lt(ord_id)) {
        return true;
    }
    if(std::is_gt(ord_id)) {
        return false;
    }
    // Then by port
    return port < other.port;
}

std::string DiscoveredService::to_uri() const {
    return "tcp://" + range_to_string(address.to_bytes(), ".") + ":" + to_string(port);
}

bool DiscoveredService::operator<(const DiscoveredService& other) const {
    // Ignore IP when sorting, we only care about the host
    auto ord_host_id = host_id <=> other.host_id;
    if(std::is_lt(ord_host_id)) {
        return true;
    }
    if(std::is_gt(ord_host_id)) {
        return false;
    }
    // Same as RegisteredService::operator<
    auto ord_id = std::to_underlying(identifier) <=> std::to_underlying(other.identifier);
    if(std::is_lt(ord_id)) {
        return true;
    }
    if(std::is_gt(ord_id)) {
        return false;
    }
    return port < other.port;
}

bool DiscoverCallbackEntry::operator<(const DiscoverCallbackEntry& other) const {
    // First sort after callback address NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto ord_callback = reinterpret_cast<std::uintptr_t>(callback) <=> reinterpret_cast<std::uintptr_t>(other.callback);
    if(std::is_lt(ord_callback)) {
        return true;
    }
    if(std::is_gt(ord_callback)) {
        return false;
    }
    // Then after service identifier to listen to
    return std::to_underlying(service_id) < std::to_underlying(other.service_id);
}

Manager* Manager::getDefaultInstance() {
    return Manager::default_manager_instance_;
}

void Manager::setAsDefaultInstance() {
    Manager::default_manager_instance_ = this;
}

Manager::Manager(const asio::ip::address_v4& brd_address,
                 const asio::ip::address_v4& any_address,
                 std::string_view group_name,
                 std::string_view host_name)
    : receiver_(any_address, CHIRP_PORT), sender_(brd_address, CHIRP_PORT), group_id_(MD5Hash(group_name)),
      host_id_(MD5Hash(host_name)), logger_("CHIRP") {

    LOG(logger_, TRACE) << "Using broadcast address " << brd_address.to_string();
    LOG(logger_, TRACE) << "Using any address " << any_address.to_string();
    LOG(logger_, DEBUG) << "Host ID for satellite " << host_name << " is " << host_id_.to_string();
    LOG(logger_, DEBUG) << "Group ID for constellation " << group_name << " is " << group_id_.to_string();
}

Manager::Manager(std::string_view brd_ip, std::string_view any_ip, std::string_view group_name, std::string_view host_name)
    : Manager(asio::ip::make_address_v4(brd_ip), asio::ip::make_address_v4(any_ip), group_name, host_name) {}

Manager::~Manager() {
    // First stop Run function
    main_loop_thread_.request_stop();
    if(main_loop_thread_.joinable()) {
        main_loop_thread_.join();
    }
    // Now unregister all services
    unregisterServices();
}

void Manager::start() {
    // jthread immediately starts on construction
    main_loop_thread_ = std::jthread(std::bind_front(&Manager::main_loop, this));
}

bool Manager::registerService(ServiceIdentifier service_id, utils::Port port) {
    const RegisteredService service {service_id, port};

    std::unique_lock registered_services_lock {registered_services_mutex_};
    const auto insert_ret = registered_services_.insert(service);
    const bool actually_inserted = insert_ret.second;

    // Lock not needed anymore
    registered_services_lock.unlock();
    if(actually_inserted) {
        send_message(OFFER, service);
    }
    return actually_inserted;
}

bool Manager::unregisterService(ServiceIdentifier service_id, utils::Port port) {
    const RegisteredService service {service_id, port};

    std::unique_lock registered_services_lock {registered_services_mutex_};
    const auto erase_ret = registered_services_.erase(service);
    const bool actually_erased = erase_ret > 0;

    // Lock not needed anymore
    registered_services_lock.unlock();
    if(actually_erased) {
        send_message(DEPART, service);
    }
    return actually_erased;
}

void Manager::unregisterServices() {
    const std::lock_guard registered_services_lock {registered_services_mutex_};
    for(auto service : registered_services_) {
        send_message(DEPART, service);
    }
    registered_services_.clear();
}

std::set<RegisteredService> Manager::getRegisteredServices() {
    const std::lock_guard registered_services_lock {registered_services_mutex_};
    return registered_services_;
}

bool Manager::registerDiscoverCallback(DiscoverCallback* callback, ServiceIdentifier service_id, std::any user_data) {
    const std::lock_guard discover_callbacks_lock {discover_callbacks_mutex_};
    const auto insert_ret = discover_callbacks_.emplace(callback, service_id, std::move(user_data));

    // Return if actually inserted
    return insert_ret.second;
}

bool Manager::unregisterDiscoverCallback(DiscoverCallback* callback, ServiceIdentifier service_id) {
    const std::lock_guard discover_callbacks_lock {discover_callbacks_mutex_};
    const auto erase_ret = discover_callbacks_.erase({callback, service_id, {}});

    // Return if actually erased
    return erase_ret > 0;
}

void Manager::unregisterDiscoverCallbacks() {
    const std::lock_guard discover_callbacks_lock {discover_callbacks_mutex_};
    discover_callbacks_.clear();
}

void Manager::forgetDiscoveredService(ServiceIdentifier identifier, message::MD5Hash host_id) {
    const std::lock_guard discovered_services_lock {discovered_services_mutex_};
    const auto service_it = std::ranges::find_if(discovered_services_, [&](const auto& service) {
        return service.host_id == host_id && service.identifier == identifier;
    });
    if(service_it != discovered_services_.end()) {
        LOG(logger_, DEBUG) << "Dropping discovered service " << to_string(identifier) << " for host id "
                            << host_id.to_string();
        call_discover_callbacks(*service_it, ServiceStatus::DEAD);
        discovered_services_.erase(service_it);
    }
}

void Manager::forgetDiscoveredServices(message::MD5Hash host_id) {
    const std::lock_guard discovered_services_lock {discovered_services_mutex_};

    const auto count = std::erase_if(discovered_services_, [&](const auto& service) {
        if(service.host_id == host_id) {
            call_discover_callbacks(service, ServiceStatus::DEAD);
            return true;
        }
        return false;
    });
    LOG(logger_, DEBUG) << "Dropped " << count << " discovered services for host id " << host_id.to_string();
}

void Manager::forgetDiscoveredServices() {
    const std::lock_guard discovered_services_lock {discovered_services_mutex_};
    discovered_services_.clear();
}

std::vector<DiscoveredService> Manager::getDiscoveredServices() {
    std::vector<DiscoveredService> ret {};
    const std::lock_guard discovered_services_lock {discovered_services_mutex_};
    std::ranges::copy(discovered_services_, std::back_inserter(ret));
    return ret;
}

std::vector<DiscoveredService> Manager::getDiscoveredServices(ServiceIdentifier service_id) {
    std::vector<DiscoveredService> ret {};
    const std::lock_guard discovered_services_lock {discovered_services_mutex_};
    for(const auto& discovered_service : discovered_services_) {
        if(discovered_service.identifier == service_id) {
            ret.push_back(discovered_service);
        }
    }
    return ret;
}

void Manager::sendRequest(ServiceIdentifier service) {
    send_message(REQUEST, {service, 0});
}

void Manager::send_message(MessageType type, RegisteredService service) {
    LOG(logger_, DEBUG) << "Sending " << to_string(type) << " for " << to_string(service.identifier) << " service on port "
                        << service.port;
    const auto asm_msg = CHIRPMessage(type, group_id_, host_id_, service.identifier, service.port).assemble();
    sender_.sendBroadcast(asm_msg);
}

void Manager::call_discover_callbacks(const DiscoveredService& discovered_service, ServiceStatus status) {
    const std::lock_guard discover_callbacks_lock {discover_callbacks_mutex_};
    std::vector<std::future<void>> futures {};
    futures.reserve(discover_callbacks_.size());
    for(const auto& callback : discover_callbacks_) {
        if(callback.service_id == discovered_service.identifier) {
            futures.emplace_back(
                std::async(std::launch::async, callback.callback, discovered_service, status, callback.user_data));
        }
    }
    for(const auto& future : futures) {
        future.wait();
    }
}

void Manager::main_loop(const std::stop_token& stop_token) {
    while(!stop_token.stop_requested()) {
        try {
            const auto raw_msg_opt = receiver_.asyncRecvBroadcast(50ms);

            // Check for timeout
            if(!raw_msg_opt.has_value()) {
                continue;
            }

            const auto& raw_msg = raw_msg_opt.value();
            auto chirp_msg = CHIRPMessage::disassemble(raw_msg.content);

            LOG(logger_, TRACE) << "Received message from " << raw_msg.address.to_string()
                                << ": group = " << chirp_msg.getGroupID().to_string()
                                << ", host = " << chirp_msg.getHostID().to_string()
                                << ", type = " << to_string(chirp_msg.getType())
                                << ", service = " << to_string(chirp_msg.getServiceIdentifier())
                                << ", port = " << chirp_msg.getPort();

            if(chirp_msg.getGroupID() != group_id_) {
                // Broadcast from different group, ignore
                continue;
            }
            if(chirp_msg.getHostID() == host_id_) {
                // Broadcast from self, ignore
                continue;
            }

            const DiscoveredService discovered_service {
                raw_msg.address, chirp_msg.getHostID(), chirp_msg.getServiceIdentifier(), chirp_msg.getPort()};

            switch(chirp_msg.getType()) {
            case REQUEST: {
                auto service_id = discovered_service.identifier;
                LOG(logger_, DEBUG) << "Received REQUEST for " << to_string(service_id) << " services";
                const std::lock_guard registered_services_lock {registered_services_mutex_};
                // Replay OFFERs for registered services with same service identifier
                for(const auto& service : registered_services_) {
                    if(service.identifier == service_id) {
                        send_message(OFFER, service);
                    }
                }
                break;
            }
            case OFFER: {
                std::unique_lock discovered_services_lock {discovered_services_mutex_};
                if(!discovered_services_.contains(discovered_service)) {
                    discovered_services_.insert(discovered_service);

                    // Unlock discovered_services_lock for user callback
                    discovered_services_lock.unlock();

                    LOG(logger_, DEBUG) << to_string(chirp_msg.getServiceIdentifier()) << " service at "
                                        << raw_msg.address.to_string() << ":" << chirp_msg.getPort() << " discovered";

                    call_discover_callbacks(discovered_service, ServiceStatus::DISCOVERED);
                }
                break;
            }
            case DEPART: {
                std::unique_lock discovered_services_lock {discovered_services_mutex_};
                if(discovered_services_.contains(discovered_service)) {
                    discovered_services_.erase(discovered_service);

                    // Unlock discovered_services_lock for user callback
                    discovered_services_lock.unlock();

                    LOG(logger_, DEBUG) << to_string(chirp_msg.getServiceIdentifier()) << " service at "
                                        << raw_msg.address.to_string() << ":" << chirp_msg.getPort() << " departed";

                    call_discover_callbacks(discovered_service, ServiceStatus::DEPARTED);
                }
                break;
            }
            default: std::unreachable();
            }
        } catch(const MessageDecodingError& error) {
            LOG(logger_, WARNING) << error.what();
            continue;
        }
    }
}
