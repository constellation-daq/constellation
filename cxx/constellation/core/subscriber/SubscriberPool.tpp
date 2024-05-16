/**
 * @file
 * @brief Subscriber pool implementation
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "SubscriberPool.hpp"

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
#include "constellation/core/logging/log.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/message/exceptions.hpp"

namespace constellation::utils {

    template <typename MESSAGE>
    SubscriberPool<MESSAGE>::SubscriberPool(chirp::ServiceIdentifier service,
                                            const std::string& logger_name,
                                            std::function<void(const MESSAGE&)> callback,
                                            std::initializer_list<std::string> default_topics)
        : service_(service), logger_(logger_name), message_callback_(std::move(callback)), default_topics_(default_topics) {

        auto* chirp_manager = chirp::Manager::getDefaultInstance();
        if(chirp_manager != nullptr) {
            // Register CHIRP callback
            chirp_manager->registerDiscoverCallback(&SubscriberPool::callback, service_, this);
            // Request currently active services
            chirp_manager->sendRequest(service_);
        }

        // Start the subscriber thread
        subscriber_thread_ = std::jthread(std::bind_front(&SubscriberPool::loop, this));
    }

    template <typename MESSAGE> SubscriberPool<MESSAGE>::~SubscriberPool() {
        auto* chirp_manager = chirp::Manager::getDefaultInstance();
        if(chirp_manager != nullptr) {
            // Unregister CHIRP discovery callback:
            chirp_manager->unregisterDiscoverCallback(&SubscriberPool::callback, service_);
        }

        // Stop the subscriber thread
        subscriber_thread_.request_stop();
        af_.test_and_set();
        af_.notify_one();

        if(subscriber_thread_.joinable()) {
            subscriber_thread_.join();
        }

        // Disconnect from all remote sockets
        disconnect_all();
    }

    template <typename MESSAGE>
    void SubscriberPool<MESSAGE>::scribe(std::string_view host, std::string_view topic, bool subscribe) {
        // Get host ID from name:
        const auto host_id = message::MD5Hash(host);

        const std::lock_guard sockets_lock {sockets_mutex_};

        const auto socket_it =
            std::ranges::find_if(sockets_, host_id, [&](const auto& s) { return s.first.host_id == host_id; });
        if(socket_it != sockets_.end()) {
            if(subscribe) {
                socket_it->second.subscribe(topic);
            } else {
                socket_it->second.unsubscribe(topic);
            }
        }
    }

    template <typename MESSAGE> void SubscriberPool<MESSAGE>::subscribe(std::string_view host, std::string_view topic) {
        scribe(host, topic, true);
    }

    template <typename MESSAGE> void SubscriberPool<MESSAGE>::unsubscribe(std::string_view host, std::string_view topic) {
        scribe(host, topic, false);
    }

    template <typename MESSAGE> void SubscriberPool<MESSAGE>::connect(const chirp::DiscoveredService& service) {
        const std::lock_guard sockets_lock {sockets_mutex_};

        // Connect
        LOG(logger_, TRACE) << "Connecting to " << service.to_uri() << "...";
        try {

            zmq::socket_t socket {context_, zmq::socket_type::sub};
            socket.connect(service.to_uri());

            // Directly subscribe to default topic list
            for(const auto& topic : default_topics_) {
                socket.set(zmq::sockopt::subscribe, topic);
            }

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

    template <typename MESSAGE> void SubscriberPool<MESSAGE>::disconnect_all() {
        const std::lock_guard sockets_lock {sockets_mutex_};

        // Unregister all sockets from the poller, then disconnect and close them.
        for(auto& [service, socket] : sockets_) {
            try {
                poller_.remove(zmq::socket_ref(socket));
                socket.disconnect(service.to_uri());
                socket.close();
            } catch(const zmq::error_t& error) {
                LOG(logger_, WARNING) << "Error disconnecting socket for " << service.to_uri() << ": " << error.what();
            }
        }
        sockets_.clear();
    }

    template <typename MESSAGE> void SubscriberPool<MESSAGE>::disconnect(const chirp::DiscoveredService& service) {
        const std::lock_guard sockets_lock {sockets_mutex_};

        // Disconnect the socket
        const auto socket_it = sockets_.find(service);
        if(socket_it != sockets_.end()) {
            LOG(logger_, TRACE) << "Disconnecting from " << service.to_uri() << "...";
            try {
                // Remove from poller
                poller_.remove(zmq::socket_ref(socket_it->second));
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

    template <typename MESSAGE>
    void SubscriberPool<MESSAGE>::callback_impl(const chirp::DiscoveredService& service, bool depart) {
        LOG(logger_, TRACE) << "Callback for " << service.to_uri() << (depart ? ", departing" : "");

        if(depart) {
            disconnect(service);
        } else {
            connect(service);
        }

        // Ping the loop thread
        af_.test_and_set();
        af_.notify_one();
    }

    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    template <typename MESSAGE>
    void SubscriberPool<MESSAGE>::callback(chirp::DiscoveredService service, bool depart, std::any user_data) {
        auto* instance = std::any_cast<SubscriberPool*>(user_data);
        instance->callback_impl(service, depart);
    }

    template <typename MESSAGE> void SubscriberPool<MESSAGE>::loop(const std::stop_token& stop_token) {
        while(!stop_token.stop_requested()) {
            std::unique_lock lock {sockets_mutex_, std::defer_lock};

            // Try to get the lock, if fails just continue
            using namespace std::literals::chrono_literals;
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
    }
} // namespace constellation::utils
