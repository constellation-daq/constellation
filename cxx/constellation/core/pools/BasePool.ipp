/**
 * @file
 * @brief Base pool implementation
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "BasePool.hpp"

#include <any>
#include <functional>
#include <mutex>
#include <stop_token>
#include <string_view>
#include <thread>
#include <utility>

#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/utils/networking.hpp"

namespace constellation::pools {

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::BasePool(std::string_view log_topic, std::function<void(MESSAGE&&)> callback)
        : pool_logger_(log_topic), message_callback_(std::move(callback)) {}

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    void BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::startPool() {
        // Start the pool thread
        pool_thread_ = std::jthread(std::bind_front(&BasePool::loop, this));

        auto* chirp_manager = chirp::Manager::getDefaultInstance();
        if(chirp_manager != nullptr) {
            // Call callback for all already discovered services
            const auto discovered_services = chirp_manager->getDiscoveredServices(SERVICE);
            for(const auto& discovered_service : discovered_services) {
                callback_impl(discovered_service, false);
            }

            // Register CHIRP callback
            chirp_manager->registerDiscoverCallback(&BasePool::callback, SERVICE, this);
            // Request currently active services
            chirp_manager->sendRequest(SERVICE);
        }
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    void BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::stopPool() {
        auto* chirp_manager = chirp::Manager::getDefaultInstance();
        if(chirp_manager != nullptr) {
            // Unregister CHIRP discovery callback:
            chirp_manager->unregisterDiscoverCallback(&BasePool::callback, SERVICE);
        }

        // Stop the pool thread
        pool_thread_.request_stop();
        if(pool_thread_.joinable()) {
            pool_thread_.join();
        }

        // Disconnect from all remote sockets
        disconnect_all();
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::~BasePool() {
        stopPool();
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    std::size_t BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::countSockets() {
        const std::lock_guard sockets_lock {sockets_mutex_};
        return sockets_.size();
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    bool BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::should_connect(const chirp::DiscoveredService& /*service*/) {
        return true;
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    void BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::socket_connected(zmq::socket_t& /*socket*/) {}

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    void BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::socket_disconnected(zmq::socket_t& /*socket*/) {}

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    void BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::checkPoolException() {
        // If exception has been thrown, disconnect from all remote sockets and propagate it
        if(exception_ptr_) {
            disconnect_all();
            std::rethrow_exception(exception_ptr_);
        }
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    void BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::connect(const chirp::DiscoveredService& service) {
        const std::lock_guard sockets_lock {sockets_mutex_};

        // Connect
        LOG(pool_logger_, TRACE) << "Connecting to " << service.to_uri() << "...";
        try {

            zmq::socket_t socket {*utils::global_zmq_context(), SOCKET_TYPE};
            socket.connect(service.to_uri());

            // Perform connection actions:
            socket_connected(socket);

            /**
             * This lambda is passed to the ZMQ active_poller_t to be called when a socket has a incoming message pending.
             * Since this is set per-socket, we can pass a reference to the currently registered socket to the lambda and
             * then directly access the socket, read the ZMQ message and pass it to the message callback.
             */
            const zmq::active_poller_t::handler_type handler = [this, sock = zmq::socket_ref(socket)](zmq::event_flags ef) {
                // Check if flags indicate the correct ZMQ event (pollin, incoming message):
                if((ef & zmq::event_flags::pollin) != zmq::event_flags::none) {
                    zmq::multipart_t zmq_msg {};
                    auto received = zmq_msg.recv(sock);
                    if(received) {
                        try {
                            message_callback_(MESSAGE::disassemble(zmq_msg));
                        } catch(const message::MessageDecodingError& error) {
                            LOG(pool_logger_, WARNING) << error.what();
                        } catch(const message::IncorrectMessageType& error) {
                            LOG(pool_logger_, WARNING) << error.what();
                        }
                    }
                }
            };

            // Register the socket with the poller
            poller_.add(socket, zmq::event_flags::pollin, handler);
            sockets_.emplace(service, std::move(socket));
            sockets_empty_.store(false);
            LOG(pool_logger_, DEBUG) << "Connected to " << service.to_uri();
        } catch(const zmq::error_t& error) {
            // The socket is emplaced in the list only on success of connection and poller registration and goes out of scope
            // when an exception is thrown. Its  calls close() automatically.
            LOG(pool_logger_, WARNING) << "Error when registering socket for " << service.to_uri() << ": " << error.what();
        }
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    void BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::disconnect_all() {
        const std::lock_guard sockets_lock {sockets_mutex_};

        // Unregister all sockets from the poller, then disconnect and close them.
        for(auto& [service, socket] : sockets_) {
            try {
                // Remove from poller
                poller_.remove(zmq::socket_ref(socket));

                // Perform disconnect actions:
                socket_disconnected(socket);

                // Disconnect and close socket
                socket.disconnect(service.to_uri());
                socket.close();
            } catch(const zmq::error_t& error) {
                LOG(pool_logger_, WARNING) << "Error disconnecting socket for " << service.to_uri() << ": " << error.what();
            }
        }
        sockets_.clear();
        sockets_empty_.store(true);
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    void BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::disconnect(const chirp::DiscoveredService& service) {
        const std::lock_guard sockets_lock {sockets_mutex_};

        // Disconnect the socket
        const auto socket_it = sockets_.find(service);
        if(socket_it != sockets_.end()) {
            LOG(pool_logger_, TRACE) << "Disconnecting from " << service.to_uri() << "...";
            try {
                // Remove from poller
                poller_.remove(zmq::socket_ref(socket_it->second));

                // Perform disconnect actions:
                socket_disconnected(socket_it->second);

                // Disconnect the socket and close it
                socket_it->second.disconnect(service.to_uri());
                socket_it->second.close();
            } catch(const zmq::error_t& error) {
                LOG(pool_logger_, WARNING)
                    << "Error disconnecting socket for " << socket_it->first.to_uri() << ": " << error.what();
            }

            sockets_.erase(socket_it);
            sockets_empty_.store(sockets_.empty());
            LOG(pool_logger_, DEBUG) << "Disconnected from " << service.to_uri();
        }
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    void BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::callback_impl(const chirp::DiscoveredService& service, bool depart) {
        LOG(pool_logger_, TRACE) << "Callback for " << service.to_uri() << (depart ? ", departing" : "");

        if(depart) {
            disconnect(service);
        } else if(should_connect(service)) {
            connect(service);
        }
    }

    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    void BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::callback(chirp::DiscoveredService service,
                                                           bool depart,
                                                           std::any user_data) {
        auto* instance = std::any_cast<BasePool*>(user_data);
        instance->callback_impl(service, depart);
    }

    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    void BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::loop(const std::stop_token& stop_token) {
        try {
            while(!stop_token.stop_requested()) {
                using namespace std::literals::chrono_literals;

                // FIXME something here gets optimized away which leads to a deadlock. Adding even a 1ns wait fixes it:
                std::this_thread::sleep_for(1ns);

                // The poller doesn't work if no socket registered
                if(sockets_empty_.load()) {
                    std::this_thread::sleep_for(50ms);
                    continue;
                }

                const std::lock_guard lock {sockets_mutex_};

                // The poller returns immediately when a socket received something, but will time out after the set period:
                poller_events_.store(poller_.wait(50ms));
            }
        } catch(...) {
            LOG(pool_logger_, DEBUG) << "Caught exception in pool thread";

            // Save exception
            exception_ptr_ = std::current_exception();
        }
    }
} // namespace constellation::pools
