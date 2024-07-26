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
#include <set>
#include <stop_token>
#include <thread>
#include <utility>

#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/utils/networking.hpp"

namespace constellation::pools {

    template <typename MESSAGE>
    BasePool<MESSAGE>::BasePool(chirp::ServiceIdentifier service,
                                std::string_view log_topic,
                                std::function<void(const MESSAGE&)> callback,
                                zmq::socket_type socket_type)
        : logger_(log_topic), service_(service), message_callback_(std::move(callback)), socket_type_(socket_type) {}

    template <typename MESSAGE> void BasePool<MESSAGE>::start() {

        // Start the pool thread
        pool_thread_ = std::jthread(std::bind_front(&BasePool::loop, this));

        auto* chirp_manager = chirp::Manager::getDefaultInstance();
        if(chirp_manager != nullptr) {
            // Register CHIRP callback
            chirp_manager->registerDiscoverCallback(&BasePool::callback, service_, this);
            // Request currently active services
            chirp_manager->sendRequest(service_);
        }
    }

    template <typename MESSAGE> BasePool<MESSAGE>::~BasePool() {
        auto* chirp_manager = chirp::Manager::getDefaultInstance();
        if(chirp_manager != nullptr) {
            // Unregister CHIRP discovery callback:
            chirp_manager->unregisterDiscoverCallback(&BasePool::callback, service_);
        }

        // Stop the pool thread
        pool_thread_.request_stop();
        af_.test_and_set();
        af_.notify_one();

        if(pool_thread_.joinable()) {
            pool_thread_.join();
        }

        // Disconnect from all remote sockets
        disconnect_all();
    }

    template <typename MESSAGE> bool BasePool<MESSAGE>::should_connect(const chirp::DiscoveredService& /*service*/) {
        return true;
    }

    template <typename MESSAGE> void BasePool<MESSAGE>::socket_connected(zmq::socket_t& /*socket*/) {}
    template <typename MESSAGE> void BasePool<MESSAGE>::socket_disconnected(zmq::socket_t& /*socket*/) {}

    template <typename MESSAGE> void BasePool<MESSAGE>::checkException() {
        // If exception has been thrown, disconnect from all remote sockets and propagate it
        if(exception_ptr_) {
            disconnect_all();
            std::rethrow_exception(exception_ptr_);
        }
    }

    template <typename MESSAGE> void BasePool<MESSAGE>::connect(const chirp::DiscoveredService& service) {
        const std::lock_guard sockets_lock {sockets_mutex_};

        // Connect
        LOG(logger_, TRACE) << "Connecting to " << service.to_uri() << "...";
        try {

            zmq::socket_t socket {*utils::global_zmq_context(), socket_type_};
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
                            const auto msg = MESSAGE::disassemble(zmq_msg);
                            message_callback_(msg);
                        } catch(const message::MessageDecodingError& error) {
                            LOG(logger_, WARNING) << error.what();
                        } catch(const message::IncorrectMessageType& error) {
                            LOG(logger_, WARNING) << error.what();
                        }
                    }
                }
            };

            // Register the socket with the poller
            poller_.add(socket, zmq::event_flags::pollin, handler);
            sockets_.emplace(service, std::move(socket));
            LOG(logger_, DEBUG) << "Connected to " << service.to_uri();
        } catch(const zmq::error_t& error) {
            // The socket is emplaced in the list only on success of connection and poller registration and goes out of scope
            // when an exception is thrown. Its  calls close() automatically.
            LOG(logger_, WARNING) << "Error when registering socket for " << service.to_uri() << ": " << error.what();
        }
    }

    template <typename MESSAGE> void BasePool<MESSAGE>::disconnect_all() {
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
                LOG(logger_, WARNING) << "Error disconnecting socket for " << service.to_uri() << ": " << error.what();
            }
        }
        sockets_.clear();
    }

    template <typename MESSAGE> void BasePool<MESSAGE>::disconnect(const chirp::DiscoveredService& service) {
        const std::lock_guard sockets_lock {sockets_mutex_};

        // Disconnect the socket
        const auto socket_it = sockets_.find(service);
        if(socket_it != sockets_.end()) {
            LOG(logger_, TRACE) << "Disconnecting from " << service.to_uri() << "...";
            try {
                // Remove from poller
                poller_.remove(zmq::socket_ref(socket_it->second));

                // Perform disconnect actions:
                socket_disconnected(socket_it->second);

                // Disconnect the socket and close it
                socket_it->second.disconnect(service.to_uri());
                socket_it->second.close();
            } catch(const zmq::error_t& error) {
                LOG(logger_, WARNING) << "Error disconnecting socket for " << socket_it->first.to_uri() << ": "
                                      << error.what();
            }

            sockets_.erase(socket_it);
            LOG(logger_, DEBUG) << "Disconnected from " << service.to_uri();
        }
    }

    template <typename MESSAGE> void BasePool<MESSAGE>::callback_impl(const chirp::DiscoveredService& service, bool depart) {
        LOG(logger_, TRACE) << "Callback for " << service.to_uri() << (depart ? ", departing" : "");

        if(depart) {
            disconnect(service);
        } else if(should_connect(service)) {
            connect(service);
        }

        // Ping the loop thread
        af_.test_and_set();
        af_.notify_one();
    }

    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    template <typename MESSAGE>
    void BasePool<MESSAGE>::callback(chirp::DiscoveredService service, bool depart, std::any user_data) {
        auto* instance = std::any_cast<BasePool*>(user_data);
        instance->callback_impl(service, depart);
    }

    template <typename MESSAGE> void BasePool<MESSAGE>::loop(const std::stop_token& stop_token) {
        try {
            while(!stop_token.stop_requested()) {
                using namespace std::literals::chrono_literals;
                std::unique_lock lock {sockets_mutex_, std::defer_lock};

                // FIXME something here gets optimized away which leads to a deadlock. Adding even a 1ns wait fixes it:
                std::this_thread::sleep_for(1ns);

                // Try to get the lock, if fails just continue
                const auto locked = lock.try_lock_for(50ms);
                if(!locked) {
                    continue;
                }

                // Poller crashes if called with no sockets attached, thus check
                if(sockets_.empty()) {
                    // Unlock so that other threads can modify sockets_
                    lock.unlock();

                    // Wait to get notified that either sockets_ was modified or a stop was requested
                    af_.wait(false);
                    af_.clear();
                    // Go to next loop iteration in case a stop was requested
                    continue;
                }

                // The poller returns immediately when a socket received something, but will time out after the set period:
                poller_.wait(50ms);
            }
        } catch(...) {
            LOG(logger_, DEBUG) << "Caught exception in pool thread";

            // Save exception
            exception_ptr_ = std::current_exception();

            // Propagate that the worker terminated
            af_.notify_one();
        }
    }
} // namespace constellation::pools
