/**
 * @file
 * @brief Tests for CHIRP manager
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <any>
#include <atomic>
#include <chrono> // IWYU pragma: keep
#include <cstddef>
#include <future>
#include <thread>
#include <utility>

#include <asio.hpp>
#include <catch2/catch_test_macros.hpp>

#include "constellation/core/chirp/BroadcastRecv.hpp"
#include "constellation/core/chirp/BroadcastSend.hpp"
#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"

using namespace constellation::chirp;
using namespace constellation::message;
using namespace std::chrono_literals;

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Sorting of registered services", "[chirp]") {
    // test self not smaller than self
    REQUIRE_FALSE(RegisteredService({DATA, 0}) < RegisteredService({DATA, 0}));
    // test service identifier takes priority over port
    REQUIRE(RegisteredService({CONTROL, 1}) < RegisteredService({DATA, 0}));
    REQUIRE_FALSE(RegisteredService({DATA, 0}) < RegisteredService({CONTROL, 1}));
    // test sort after port if service identifier the same
    REQUIRE(RegisteredService({DATA, 0}) < RegisteredService({DATA, 1}));
}

TEST_CASE("Sorting of discovered services", "[chirp]") {
    auto id1 = MD5Hash("a");
    auto id2 = MD5Hash("b");
    auto ip1 = asio::ip::make_address_v4("1.2.3.4");
    auto ip2 = asio::ip::make_address_v4("4.3.2.1");

    // test self not smaller than self
    REQUIRE_FALSE(DiscoveredService({ip1, id1, DATA, 0}) < DiscoveredService({ip1, id1, DATA, 0}));
    // test ip does not change sorting
    REQUIRE_FALSE(DiscoveredService({ip1, id1, DATA, 0}) < DiscoveredService({ip2, id1, DATA, 0}));
    REQUIRE_FALSE(DiscoveredService({ip2, id1, DATA, 0}) < DiscoveredService({ip1, id1, DATA, 0}));
    // test host takes priority
    REQUIRE(DiscoveredService({ip1, id1, DATA, 1}) < DiscoveredService({ip1, id2, CONTROL, 0}));
    REQUIRE_FALSE(DiscoveredService({ip1, id2, CONTROL, 0}) < DiscoveredService({ip1, id1, DATA, 1}));
    // test service identifier takes priority if same host
    REQUIRE(DiscoveredService({ip1, id1, CONTROL, 1}) < DiscoveredService({ip1, id1, DATA, 0}));
    REQUIRE_FALSE(DiscoveredService({ip1, id1, DATA, 0}) < DiscoveredService({ip1, id1, CONTROL, 1}));
    // test port takes priority if same host and service identifier
    REQUIRE(DiscoveredService({ip1, id1, DATA, 0}) < DiscoveredService({ip1, id1, DATA, 1}));
}

TEST_CASE("Sorting of discover callbacks", "[chirp]") {
    auto* cb1 = reinterpret_cast<DiscoverCallback*>(1); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    auto* cb2 = reinterpret_cast<DiscoverCallback*>(2); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    auto ud1 = std::make_any<int>(1);
    auto ud2 = std::make_any<int>(2);

    // test self not smaller than self
    REQUIRE_FALSE(DiscoverCallbackEntry({cb1, DATA, ud1}) < DiscoverCallbackEntry({cb1, DATA, ud1}));
    // test user data does not change sorting
    REQUIRE_FALSE(DiscoverCallbackEntry({cb1, DATA, ud1}) < DiscoverCallbackEntry({cb1, DATA, ud2}));
    REQUIRE_FALSE(DiscoverCallbackEntry({cb1, DATA, ud2}) < DiscoverCallbackEntry({cb1, DATA, ud1}));
    // test callback address takes priority
    REQUIRE(DiscoverCallbackEntry({cb1, DATA, ud1}) < DiscoverCallbackEntry({cb2, CONTROL, ud1}));
    REQUIRE_FALSE(DiscoverCallbackEntry({cb2, CONTROL, ud1}) < DiscoverCallbackEntry({cb1, DATA, ud1}));
    // test service identifier takes priority if same callback address
    REQUIRE(DiscoverCallbackEntry({cb1, CONTROL, ud1}) < DiscoverCallbackEntry({cb1, DATA, ud1}));
}

TEST_CASE("Register services in CHIRP manager", "[chirp][chirp::manager]") {
    Manager manager {"0.0.0.0", "0.0.0.0", "group1", "sat1"};

    // test that first register works
    REQUIRE(manager.registerService(CONTROL, 23999));
    // test that second register does not work
    REQUIRE_FALSE(manager.registerService(CONTROL, 23999));
    // test that unregistering works
    REQUIRE(manager.unregisterService(CONTROL, 23999));
    // test that unregistering for not registered service does not work
    REQUIRE_FALSE(manager.unregisterService(CONTROL, 23999));
    // test unregister all services
    manager.registerService(CONTROL, 23999);
    manager.registerService(CONTROL, 24000);
    REQUIRE(manager.getRegisteredServices().size() == 2);
    manager.unregisterServices();
    REQUIRE(manager.getRegisteredServices().empty());
}

TEST_CASE("Register callbacks in CHIRP manager", "[chirp][chirp::manager]") {
    Manager manager {"0.0.0.0", "0.0.0.0", "group1", "sat1"};

    // DiscoverCallback signature NOLINTNEXTLINE(performance-unnecessary-value-param)
    auto callback = [](DiscoveredService, ServiceStatus, std::any) {};

    // test that first register works
    REQUIRE(manager.registerDiscoverCallback(callback, CONTROL, nullptr));
    // test that second register does not work
    REQUIRE_FALSE(manager.registerDiscoverCallback(callback, CONTROL, nullptr));
    // test that unregistering works
    REQUIRE(manager.unregisterDiscoverCallback(callback, CONTROL));
    // test that unregistering for not registered service does not work
    REQUIRE_FALSE(manager.unregisterDiscoverCallback(callback, CONTROL));

    // coverage test for unregister all services
    manager.registerDiscoverCallback(callback, CONTROL, nullptr);
    manager.registerDiscoverCallback(callback, HEARTBEAT, nullptr);
    manager.unregisterDiscoverCallbacks();
}

TEST_CASE("Get async timeout in CHIRP manager", "[chirp][chirp::manager]") {
    Manager manager {"0.0.0.0", "0.0.0.0", "group1", "sat1"};
    manager.start();
    // This is purely a coverage test to ensure that the async receive works
    std::this_thread::sleep_for(100ms);
}

TEST_CASE("Ignore CHIRP message from other group in CHIRP manager", "[chirp][chirp::manager]") {
    BroadcastSend sender {"0.0.0.0", CHIRP_PORT};
    Manager manager {"0.0.0.0", "0.0.0.0", "group1", "sat1"};
    manager.start();

    const auto asm_msg = CHIRPMessage(OFFER, "group2", "sat2", CONTROL, 23999).assemble();
    sender.sendBroadcast(asm_msg);

    REQUIRE(manager.getDiscoveredServices().empty());
}

TEST_CASE("Ignore CHIRP message from self in CHIRP manager", "[chirp][chirp::manager]") {
    BroadcastSend sender {"0.0.0.0", CHIRP_PORT};
    Manager manager {"0.0.0.0", "0.0.0.0", "group1", "sat1"};
    manager.start();

    const auto asm_msg = CHIRPMessage(OFFER, "group1", "sat1", CONTROL, 23999).assemble();
    sender.sendBroadcast(asm_msg);

    REQUIRE(manager.getDiscoveredServices().empty());
}

TEST_CASE("Discover services in CHIRP manager", "[chirp][chirp::manager]") {
    Manager manager1 {"0.0.0.0", "0.0.0.0", "group1", "sat1"};
    Manager manager2 {"0.0.0.0", "0.0.0.0", "group1", "sat2"};
    manager2.start();

    // Register service, should send OFFER
    manager1.registerService(DATA, 24000);
    // Wait a bit ensure we received the message
    std::this_thread::sleep_for(100ms);
    // Test that we discovered the service
    const auto services_1 = manager2.getDiscoveredServices();
    REQUIRE(services_1.size() == 1);

    // Test that message is correct
    REQUIRE(services_1[0].host_id == manager1.getHostID());
    REQUIRE(services_1[0].address == asio::ip::make_address_v4("127.0.0.1"));
    REQUIRE(services_1[0].identifier == DATA);
    REQUIRE(services_1[0].port == 24000);

    // Register other services
    manager1.registerService(MONITORING, 65000);
    manager1.registerService(HEARTBEAT, 65001);
    std::this_thread::sleep_for(100ms);

    // Test that we discovered the services
    REQUIRE(manager2.getDiscoveredServices().size() == 3);
    // Unregister a service
    manager1.unregisterService(MONITORING, 65000);
    std::this_thread::sleep_for(100ms);
    // Test that we discovered DEPART message
    REQUIRE(manager2.getDiscoveredServices().size() == 2);
    // Now test that we can filter a service category
    REQUIRE(manager2.getDiscoveredServices(HEARTBEAT).size() == 1);
    // Test that we can forget services
    manager2.forgetDiscoveredServices();
    REQUIRE(manager2.getDiscoveredServices().empty());

    // Register new services
    manager1.unregisterServices();
    manager1.registerService(CONTROL, 40001);
    manager1.registerService(DATA, 40002);
    std::this_thread::sleep_for(100ms);
    // Test that we discovered services
    REQUIRE(manager2.getDiscoveredServices().size() == 2);
    // Unregister all services
    manager1.unregisterServices();
    std::this_thread::sleep_for(100ms);
    // Test that we discovered DEPART messages
    REQUIRE(manager2.getDiscoveredServices().empty());
}

TEST_CASE("Execute callbacks in CHIRP manager", "[chirp][chirp::manager]") {
    Manager manager1 {"0.0.0.0", "0.0.0.0", "group1", "sat1"};
    Manager manager2 {"0.0.0.0", "0.0.0.0", "group1", "sat2"};
    manager2.start();

    // Callback test struct to test if callback was properly executed
    struct CBTest {
        ServiceStatus status {};
        DiscoveredService service {};
        std::atomic_bool executed {false};
    };
    CBTest cb_test_data {};

    // Create a callback, use pointer to access test variable
    auto callback = [](DiscoveredService service, ServiceStatus status, std::any cb_info) {
        auto* cb_test_data_p = std::any_cast<CBTest*>(cb_info);
        cb_test_data_p->status = status;
        cb_test_data_p->service = std::move(service);
        // Set callback as executed
        cb_test_data_p->executed = true;
    };

    // Register callback for CONTROL
    manager2.registerDiscoverCallback(callback, CONTROL, &cb_test_data);
    // Register CONTROL service
    manager1.registerService(CONTROL, 50100);
    // Wait for execution of callback
    while(!cb_test_data.executed) {
        std::this_thread::sleep_for(1ms);
    }
    cb_test_data.executed = false;
    // Test that we correct OFFER callback
    REQUIRE(cb_test_data.status == ServiceStatus::DISCOVERED);
    REQUIRE(cb_test_data.service.identifier == CONTROL);
    REQUIRE(cb_test_data.service.port == 50100);

    // Unregister service
    manager1.unregisterService(CONTROL, 50100);
    // Wait for execution of callback
    while(!cb_test_data.executed) {
        std::this_thread::sleep_for(1ms);
    }
    cb_test_data.executed = false;
    // Test that we got DEPART callback
    REQUIRE(cb_test_data.status == ServiceStatus::DEPARTED);

    // Forget service of a host:
    manager1.registerService(CONTROL, 50100);
    // Wait for execution of callback
    while(!cb_test_data.executed) {
        std::this_thread::sleep_for(1ms);
    }
    cb_test_data.executed = false;
    manager2.forgetDiscoveredService(CONTROL, cb_test_data.service.host_id);
    // Wait for execution of callback
    while(!cb_test_data.executed) {
        std::this_thread::sleep_for(1ms);
    }
    cb_test_data.executed = false;
    // Test that we got DEAD callback
    REQUIRE(cb_test_data.status == ServiceStatus::DEAD);

    // Unregister callback
    manager2.unregisterDiscoverCallback(callback, CONTROL);
    // Register CONTROL service
    manager1.registerService(CONTROL, 50100);
    // Wait a bit to check for execution of callback
    std::this_thread::sleep_for(100ms);
    // Test that callback was not executed
    REQUIRE_FALSE(cb_test_data.executed);

    // Register callback for HEARTBEAT and MONITORING
    manager2.registerDiscoverCallback(callback, HEARTBEAT, &cb_test_data);
    manager2.registerDiscoverCallback(callback, MONITORING, &cb_test_data);
    // Register HEARTBEAT service
    manager1.registerService(HEARTBEAT, 50200);
    // Wait for execution of callback
    while(!cb_test_data.executed) {
        std::this_thread::sleep_for(1ms);
    }
    cb_test_data.executed = false;
    // Test that we got HEARTBEAT callback
    REQUIRE(cb_test_data.service.identifier == HEARTBEAT);
    // Register MONITORING service
    manager1.registerService(MONITORING, 50300);
    // Wait for execution of callback
    while(!cb_test_data.executed) {
        std::this_thread::sleep_for(1ms);
    }
    cb_test_data.executed = false;
    // Test that we got MONITORING callback
    REQUIRE(cb_test_data.service.identifier == MONITORING);

    // Unregister all callbacks
    manager2.unregisterDiscoverCallbacks();
    // Unregister all services
    manager1.unregisterServices();
    // Wait a bit to check for execution of callback
    std::this_thread::sleep_for(100ms);
    // Test that callback was not executed
    REQUIRE_FALSE(cb_test_data.executed);
}

TEST_CASE("Send CHIRP requests in CHIRP manager", "[chirp][chirp::manager]") {
    Manager manager {"0.0.0.0", "0.0.0.0", "group1", "sat1"};
    BroadcastRecv receiver {"0.0.0.0", CHIRP_PORT};
    // Note: it seems we have to construct receiver after manager, else we do not receive messages
    // Why? we can only have one working recv binding to the same socket per process unfortunately :/

    // Start listening for request message
    auto raw_msg_fut = std::async(&BroadcastRecv::recvBroadcast, &receiver);
    // Send request
    manager.sendRequest(CONTROL);
    // Receive message
    const auto raw_msg = raw_msg_fut.get();
    auto msg_from_manager = CHIRPMessage::disassemble(raw_msg.content);
    // Check message
    REQUIRE(msg_from_manager.getType() == REQUEST);
    REQUIRE(msg_from_manager.getServiceIdentifier() == CONTROL);
    REQUIRE(msg_from_manager.getPort() == 0);
}

TEST_CASE("Receive CHIRP requests in CHIRP manager", "[chirp][chirp::manager]") {
    Manager manager {"0.0.0.0", "0.0.0.0", "group1", "sat1"};
    BroadcastSend sender {"0.0.0.0", CHIRP_PORT};
    // Note: we cannot test if an offer is actually replied, see `test_manager_send_request`

    // Register service
    manager.start();
    manager.registerService(CONTROL, 45454);
    // Send requests
    const auto asm_msg_a = CHIRPMessage(REQUEST, "group1", "sat2", CONTROL, 0).assemble();
    const auto asm_msg_b = CHIRPMessage(REQUEST, "group1", "sat2", DATA, 0).assemble();
    sender.sendBroadcast(asm_msg_a);
    sender.sendBroadcast(asm_msg_b);
    // Wait a bit ensure we received the message
    std::this_thread::sleep_for(100ms);

    // If everything worked, the corresponding lines should be marked as executed in coverage
}

TEST_CASE("Detect incorrect CHIRP message in CHIRP manager", "[chirp][chirp::manager]") {
    BroadcastSend sender {"0.0.0.0", CHIRP_PORT};
    Manager manager {"0.0.0.0", "0.0.0.0", "group1", "sat1"};
    manager.start();

    // Create invalid message
    auto asm_msg = CHIRPMessage(REQUEST, "group1", "sat2", CONTROL, 0).assemble();
    asm_msg[0] = std::byte('X');
    // Send message
    sender.sendBroadcast(asm_msg);
    // Wait a bit ensure we received the message
    std::this_thread::sleep_for(100ms);

    // If everything worked, the corresponding lines should be marked as executed in coverage
}

TEST_CASE("Default CHIRP manager instance", "[chirp][chirp::manager]") {
    // No default manager if not set
    REQUIRE(Manager::getDefaultInstance() == nullptr);

    // Set manager as default instance
    Manager manager {"0.0.0.0", "0.0.0.0", "group1", "sat1"};
    manager.setAsDefaultInstance();

    // Ensure static variable is set correctly
    REQUIRE(Manager::getDefaultInstance() == &manager);
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
