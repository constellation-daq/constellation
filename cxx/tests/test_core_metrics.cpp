/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <numbers>
#include <optional>
#include <thread>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/log/SinkManager.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/metrics/Metric.hpp"
#include "constellation/core/metrics/MetricsManager.hpp"
#include "constellation/core/metrics/stat.hpp"
#include "constellation/core/pools/SubscriberPool.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"

#include "chirp_mock.hpp"

using namespace constellation::chirp;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::metrics;
using namespace constellation::pools;
using namespace constellation::protocol::CHIRP;
using namespace constellation::utils;
using namespace std::chrono_literals;

class MetricsReceiver : public SubscriberPool<CMDP1StatMessage, ServiceIdentifier::MONITORING> {
public:
    using SubscriberPoolT = SubscriberPool<CMDP1StatMessage, ServiceIdentifier::MONITORING>;
    MetricsReceiver()
        : SubscriberPoolT("STAT", [this](CMDP1StatMessage&& msg) {
              const std::lock_guard last_message_lock {last_message_mutex_};
              last_message_ = std::make_shared<CMDP1StatMessage>(std::move(msg));
              last_message_updated_.store(true);
          }) {}
    void waitSubscription() {
        while(!subscribed_.load()) {
            std::this_thread::sleep_for(50ms);
        }
        subscribed_.store(false);
    }
    void waitNextMessage() {
        while(!last_message_updated_.load()) {
            std::this_thread::sleep_for(50ms);
        }
        last_message_updated_.store(false);
    }
    std::shared_ptr<CMDP1StatMessage> getLastMessage() {
        const std::lock_guard last_message_lock {last_message_mutex_};
        return last_message_;
    }

protected:
    void host_connected(const DiscoveredService& service) final {
        subscribe(service.host_id, "");
        subscribed_.store(true);
    }

private:
    std::atomic_bool subscribed_ {false};
    std::atomic_bool last_message_updated_ {false};
    std::mutex last_message_mutex_;
    std::shared_ptr<CMDP1StatMessage> last_message_;
};

TEST_CASE("Registering and unregistering metrics", "[core][metrics]") {
    auto& metrics_manager = MetricsManager::getInstance();

    // Register metrics
    metrics_manager.registerMetric("TEST", "t", MetricType::LAST_VALUE, "description");
    metrics_manager.registerTimedMetric("TEST_T", "t", MetricType::LAST_VALUE, "description", 100ms, []() { return 0; });

    // Overwrite registered metric
    metrics_manager.registerMetric("TEST", "u", MetricType::LAST_VALUE, "description");
    metrics_manager.registerTimedMetric("TEST_T", "t", MetricType::LAST_VALUE, "description", 100ms, []() { return 1; });

    // Unregister metric
    metrics_manager.unregisterMetric("TEST");
    metrics_manager.unregisterMetric("TEST_T");

    // Unregister non-registered metric
    metrics_manager.unregisterMetric("TEST_2");
}

TEST_CASE("Receive triggered metric", "[core][metrics]") {
    auto& metrics_manager = MetricsManager::getInstance();

    auto chirp_manager = create_chirp_manager();
    auto metrics_receiver = MetricsReceiver();
    metrics_receiver.startPool();

    // Mock service and wait until subscribed
    chirp_mock_service("Sender", ServiceIdentifier::MONITORING, SinkManager::getInstance().getCMDPPort());
    // TODO(stephan.lachnit): if subscription check is implemented, we need enable enableCMDPSending()
    //                        this also requires making the chirp manager static in create_chirp_manager()
    metrics_receiver.waitSubscription();

    // Register new metric
    metrics_manager.registerMetric("TEST", "t", MetricType::LAST_VALUE, "description");
    // Trigger metric
    metrics_manager.triggerMetric("TEST", 0);
    // Trigger unregistered metric (does nothing)
    metrics_manager.triggerMetric("TEST_2", 1);
    // Wait until metric is received
    metrics_receiver.waitNextMessage();
    // Check that metric decoded correctly
    const auto last_message = metrics_receiver.getLastMessage();
    REQUIRE(last_message->getMetric().getMetric()->name() == "TEST");
    REQUIRE(last_message->getMetric().getMetric()->unit() == "t");
    REQUIRE(last_message->getMetric().getMetric()->type() == MetricType::LAST_VALUE);
    REQUIRE(last_message->getMetric().getValue().get<int>() == 0);

    metrics_receiver.stopPool();
    metrics_manager.unregisterMetrics();
}

TEST_CASE("Receive with STAT macros", "[core][metrics]") {
    auto& metrics_manager = MetricsManager::getInstance();

    auto chirp_manager = create_chirp_manager();
    auto metrics_receiver = MetricsReceiver();
    metrics_receiver.startPool();

    // Mock service and wait until subscribed
    chirp_mock_service("Sender", ServiceIdentifier::MONITORING, SinkManager::getInstance().getCMDPPort());
    metrics_receiver.waitSubscription();

    // Register metrics
    metrics_manager.registerMetric("STAT", "counts", MetricType::LAST_VALUE, "description");
    metrics_manager.registerMetric("STAT_IF", "counts", MetricType::LAST_VALUE, "description");
    metrics_manager.registerMetric("STAT_NTH", "counts", MetricType::LAST_VALUE, "description");
    metrics_manager.registerMetric("STAT_T", "counts", MetricType::LAST_VALUE, "description");

    // Trigger metric with macro
    STAT("STAT", 1);
    metrics_receiver.waitNextMessage();
    REQUIRE(metrics_receiver.getLastMessage()->getMetric().getValue().get<int>() == 1);

    // Trigger metric with condition
    STAT_IF("STAT_IF", 2, true);
    STAT_IF("STAT_IF", 3, false);
    metrics_receiver.waitNextMessage();
    REQUIRE(metrics_receiver.getLastMessage()->getMetric().getValue().get<int>() == 2);

    // Trigger metric every nth call
    int nth_count = 0;
    for(int n = 0; n < 12; ++n) {
        STAT_NTH("STAT_NTH", ++nth_count, 3);
    }
    REQUIRE(nth_count == 4);

    // Trigger metric at most every t seconds
    int t_count = 0;
    for(int n = 0; n < 5; ++n) {
        STAT_T("STAT_T", ++t_count, 10s);
        std::this_thread::sleep_for(10ms);
    }
    REQUIRE(t_count == 1);

    metrics_receiver.stopPool();
    metrics_manager.unregisterMetrics();
}

TEST_CASE("Receive timed metric", "[core][metrics]") {
    auto& metrics_manager = MetricsManager::getInstance();

    auto chirp_manager = create_chirp_manager();
    auto metrics_receiver = MetricsReceiver();
    metrics_receiver.startPool();

    // Mock service and wait until subscribed
    chirp_mock_service("Sender", ServiceIdentifier::MONITORING, SinkManager::getInstance().getCMDPPort());
    metrics_receiver.waitSubscription();

    // Register timed metric
    metrics_manager.registerTimedMetric("TIMED", "t", MetricType::LAST_VALUE, "description", 10ms, []() { return 3.14; });

    // Receive metric
    std::this_thread::sleep_for(50ms);
    metrics_receiver.waitNextMessage();
    REQUIRE(metrics_receiver.getLastMessage()->getMetric().getValue().get<double>() == 3.14);

    metrics_receiver.stopPool();
    metrics_manager.unregisterMetrics();
}

TEST_CASE("Receive timed metric with optional", "[core][metrics]") {
    auto& metrics_manager = MetricsManager::getInstance();

    auto chirp_manager = create_chirp_manager();
    auto metrics_receiver = MetricsReceiver();
    metrics_receiver.startPool();

    // Mock service and wait until subscribed
    chirp_mock_service("Sender", ServiceIdentifier::MONITORING, SinkManager::getInstance().getCMDPPort());
    metrics_receiver.waitSubscription();

    // Register timed metric
    std::atomic_bool nullopt = false;
    std::mutex value_mutex;
    double value = std::numbers::phi;
    metrics_manager.registerTimedMetric(
        "TIMED", "t", MetricType::LAST_VALUE, "description", 10ms, [&]() -> std::optional<double> {
            const std::lock_guard value_lock {value_mutex};
            return nullopt.load() ? std::nullopt : std::optional(value);
        });

    // Receive metric, first time triggered immediately
    metrics_receiver.waitNextMessage();
    REQUIRE(metrics_receiver.getLastMessage()->getMetric().getValue().get<double>() == std::numbers::phi);

    // Disable sending and adjust value
    nullopt.store(true);
    {
        const std::lock_guard value_lock {value_mutex};
        value = std::numbers::e;
    }

    // Ensure last received message is still at phi
    std::this_thread::sleep_for(100ms);
    REQUIRE(metrics_receiver.getLastMessage()->getMetric().getValue().get<double>() == std::numbers::phi);

    // Adjust value and enable sending again
    {
        const std::lock_guard value_lock {value_mutex};
        value = std::numbers::pi;
    }
    nullopt.store(false);

    // Check value now at pi
    metrics_receiver.waitNextMessage();
    REQUIRE(metrics_receiver.getLastMessage()->getMetric().getValue().get<double>() == std::numbers::pi);

    metrics_receiver.stopPool();
    metrics_manager.unregisterMetrics();
}
