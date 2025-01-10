/**
 * @file
 * @brief Command dispatcher for user commands
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "Satellite.hpp" // NOLINT(misc-header-include-cycle)

#include <chrono>
#include <concepts>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <utility>

#include "constellation/core/metrics/Metric.hpp"
#include "constellation/core/metrics/MetricsManager.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"

namespace constellation::satellite {

    inline void
    Satellite::register_metric(std::string name, std::string unit, metrics::MetricType type, std::string description) {
        metrics::MetricsManager::getInstance().registerMetric(
            std::move(name), std::move(unit), type, std::move(description));
    }

    template <typename C>
        requires std::invocable<C>
    inline void Satellite::register_timed_metric(std::string name,
                                                 std::string unit,
                                                 metrics::MetricType type,
                                                 std::string description,
                                                 std::chrono::steady_clock::duration interval,
                                                 C value_callback) {
        metrics::MetricsManager::getInstance().registerTimedMetric(
            std::move(name), std::move(unit), type, std::move(description), interval, std::move(value_callback));
    }

    template <typename C>
        requires std::invocable<C>
    inline void Satellite::register_timed_metric(std::string name,
                                                 std::string unit,
                                                 metrics::MetricType type,
                                                 std::string description,
                                                 std::chrono::steady_clock::duration interval,
                                                 std::set<protocol::CSCP::State> allowed_states,
                                                 C value_callback) {
        metrics::MetricsManager::getInstance().registerTimedMetric(
            std::move(name),
            std::move(unit),
            type,
            std::move(description),
            interval,
            [this, allowed_states = std::move(allowed_states), value_callback = std::move(value_callback)]() {
                std::optional<std::invoke_result_t<C>> retval = std::nullopt;
                if(allowed_states.contains(getState())) {
                    retval = value_callback();
                }
                return retval;
            });
    }

} // namespace constellation::satellite
