/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include <catch2/catch_test_macros.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/pools/SubscriberPool.hpp"
#include "constellation/core/utils/networking.hpp"

#include "chirp_mock.hpp"

using namespace constellation;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::pools;
using namespace constellation::utils;

class CMDPSender {
public:
    CMDPSender(std::string name)
        : name_(std::move(name)), pub_socket_(*global_zmq_context(), zmq::socket_type::xpub),
          port_(bind_ephemeral_port(pub_socket_)) {}

    Port getPort() const { return port_; }

    std::string_view getName() const { return name_; }

    void sendLogMessage(Level level, std::string topic, std::string message) {
        auto msg = CMDP1LogMessage(level, std::move(topic), {"CMDPSender.s1"}, std::move(message));
        msg.assemble().send(pub_socket_);
    }

    void sendRaw(zmq::multipart_t& msg) { msg.send(pub_socket_); }

    zmq::multipart_t recv() {
        zmq::multipart_t recv_msg {};
        recv_msg.recv(pub_socket_);
        return recv_msg;
    }

    bool canRecv() {
        zmq::message_t msg {};
        pub_socket_.set(zmq::sockopt::rcvtimeo, 200);
        auto recv_res = pub_socket_.recv(msg);
        pub_socket_.set(zmq::sockopt::rcvtimeo, -1);
        return recv_res.has_value();
    }

private:
    std::string name_;
    zmq::socket_t pub_socket_;
    Port port_;
};

class TestPool : public SubscriberPool<CMDP1Message, chirp::MONITORING> {
public:
    using SubscriberPoolT = SubscriberPool<CMDP1Message, chirp::MONITORING>;

    TestPool() : SubscriberPoolT("POOL", {}) {}

    [[nodiscard]] std::future<std::cv_status> waitCallback() {
        auto callback_fut = std::async(std::launch::async, [&]() {
            std::unique_lock pseudo_lock {pesudo_mutex_};
            const auto cv_status = cv_.wait_for(pseudo_lock, std::chrono::seconds(1));
            return cv_status;
        });
        // Give a bit of time to start the thread
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return callback_fut;
    }

protected:
    void host_connected(const chirp::DiscoveredService& /*service*/) final {
        const std::lock_guard pseudo_lock {pesudo_mutex_};
        cv_.notify_one();
    }

    void host_disconnected(const chirp::DiscoveredService& /*service*/) final {
        const std::lock_guard pseudo_lock {pesudo_mutex_};
        cv_.notify_one();
    }

private:
    std::mutex pesudo_mutex_;
    std::condition_variable cv_;
};

namespace {
    bool check_sub_message(zmq::message_t msg, bool subscribe, std::string_view topic) {
        // First byte is subscribe bool
        const auto msg_subscribe = static_cast<bool>(*msg.data<uint8_t>());
        if(msg_subscribe != subscribe) {
            return false;
        }
        // Rest is subscription topic
        auto msg_topic = msg.to_string_view();
        msg_topic.remove_prefix(1);
        return msg_topic == topic;
    }
} // namespace

TEST_CASE("Message callback", "[core][core::pools]") {
    // Create CHIRP manager for monitoring service discovery
    auto chirp_manager = create_chirp_manager();

    // Callback: move to shared_ptr
    std::mutex msg_mutex {};
    std::condition_variable cv {};
    std::shared_ptr<CMDP1LogMessage> log_msg {nullptr};
    auto callback = [&](CMDP1Message&& msg) {
        const std::lock_guard msg_lock {msg_mutex};
        log_msg = std::make_shared<CMDP1LogMessage>(std::move(msg));
        cv.notify_all();
    };

    // Start pool
    auto pool = SubscriberPool<CMDP1Message, chirp::MONITORING>("pool", std::move(callback));
    pool.startPool();

    // Start the sender and mock via chirp
    auto sender = CMDPSender("CMDPSender.s1");
    chirp_mock_service(sender.getName(), chirp::MONITORING, sender.getPort());

    // Subscribe to LOG messages
    pool.subscribe("LOG");

    // Check that we got subscription message
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG"));

    // Send log message
    sender.sendLogMessage(Level::STATUS, "", "test");
    std::unique_lock msg_lock {msg_mutex};
    const auto cv_status = cv.wait_for(msg_lock, std::chrono::seconds(1));
    REQUIRE(cv_status == std::cv_status::no_timeout);

    // Check message
    REQUIRE(log_msg != nullptr);
    REQUIRE(log_msg->getLogLevel() == Level::STATUS);
    REQUIRE(log_msg->getLogMessage() == "test");

    msg_lock.unlock();
    pool.stopPool();
}

TEST_CASE("Disconnect", "[core][core::pools]") {
    // Create CHIRP manager for monitoring service discovery
    auto chirp_manager = create_chirp_manager();

    // Start pool
    auto pool = TestPool();
    pool.startPool();

    // Get future for socket_connected callback
    auto connected_fut = pool.waitCallback();

    // Start the sender and mock via chirp
    auto sender = CMDPSender("CMDPSender.s1");
    chirp_mock_service(sender.getName(), chirp::MONITORING, sender.getPort());

    // Wait until socket is connected
    REQUIRE(connected_fut.get() == std::cv_status::no_timeout);

    // Get future for socket_disconnected callback
    auto disconnected_fut = pool.waitCallback();

    // Disconnect via chirp
    chirp_mock_service(sender.getName(), chirp::MONITORING, sender.getPort(), false);

    // Wait until socket is disconnected
    REQUIRE(disconnected_fut.get() == std::cv_status::no_timeout);

    // Subscribe to new topic
    pool.subscribe("LOG");

    // Check that we did not subscription message since disconnected
    REQUIRE_FALSE(sender.canRecv());

    pool.stopPool();
}

TEST_CASE("Sending and receiving subscriptions", "[core][core::pools]") {
    // Create CHIRP manager for monitoring service discovery
    auto chirp_manager = create_chirp_manager();

    // Start pool
    auto pool = SubscriberPool<CMDP1Message, chirp::MONITORING>("pool", {});
    pool.startPool();

    // Start the senders and mock via chirp
    auto sender1 = CMDPSender("CMDPSender.s1");
    chirp_mock_service(sender1.getName(), chirp::MONITORING, sender1.getPort());
    auto sender2 = CMDPSender("CMDPSender.s2");
    chirp_mock_service(sender2.getName(), chirp::MONITORING, sender2.getPort());

    // Subscribe to topic
    pool.subscribe("LOG/STATUS");
    REQUIRE(check_sub_message(sender1.recv().pop(), true, "LOG/STATUS"));
    REQUIRE(check_sub_message(sender2.recv().pop(), true, "LOG/STATUS"));

    // Unsubscribe from topic
    pool.unsubscribe("LOG/STATUS");
    REQUIRE(check_sub_message(sender1.recv().pop(), false, "LOG/STATUS"));
    REQUIRE(check_sub_message(sender2.recv().pop(), false, "LOG/STATUS"));

    pool.stopPool();
}

TEST_CASE("Sending and receiving subscriptions, single host", "[core][core::pools]") {
    // Create CHIRP manager for monitoring service discovery
    auto chirp_manager = create_chirp_manager();

    // Start pool
    auto pool = SubscriberPool<CMDP1Message, chirp::MONITORING>("pool", {});
    pool.startPool();

    // Start the senders and mock via chirp
    auto sender1 = CMDPSender("CMDPSender.s1");
    chirp_mock_service(sender1.getName(), chirp::MONITORING, sender1.getPort());
    auto sender2 = CMDPSender("CMDPSender.s2");
    chirp_mock_service(sender2.getName(), chirp::MONITORING, sender2.getPort());

    // Subscribing / unsubscribing from non-existing sender is fine
    pool.subscribe("fake1", "LOG");
    pool.unsubscribe("fake2", "LOG");

    // Subscribe to topic
    pool.subscribe(sender1.getName(), "LOG/STATUS");
    REQUIRE(check_sub_message(sender1.recv().pop(), true, "LOG/STATUS"));
    REQUIRE_FALSE(sender2.canRecv());

    // Unsubscribe from topic
    pool.unsubscribe(sender1.getName(), "LOG/STATUS");
    REQUIRE(check_sub_message(sender1.recv().pop(), false, "LOG/STATUS"));
    REQUIRE_FALSE(sender2.canRecv());

    pool.stopPool();
}
