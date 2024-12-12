/**
 * @file
 * @brief Metric class
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "constellation/build.hpp"
#include "constellation/core/config/Value.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"

namespace constellation::metrics {

    /** Metrics types */
    enum class MetricType : std::uint8_t {
        /** Always keep the latest value, replace earlier ones */
        LAST_VALUE = 1,
        /** Sum every new value to previously received ones */
        ACCUMULATE = 2,
        /** Calculate the average value */
        AVERAGE = 3,
        /** Calculate the rate from the value over a given time interval */
        RATE = 4,
    };
    using enum MetricType;

    /**
     * @brief This class represents a metric for telemetry or data quality monitoring
     *
     * @details It comprises a name, a unit and a type. The type defines how the value should be treated, i.e. if always the
     *          last transmitted value should be displayed, if an average of the values should be calculated or if they
     *          should be accumulated.
     */
    class Metric {
    public:
        /**
         * @brief Metric constructor
         *
         * @param name Name of the metric
         * @param unit Unit of the metric as human readable string
         * @param type Type of the metric
         */
        Metric(std::string name, std::string unit, MetricType type)
            : name_(std::move(name)), unit_(std::move(unit)), type_(type) {}

        /**
         * @brief Obtain the name of the metrics
         * @return Name of the metric
         */
        std::string_view name() const { return name_; }

        /**
         * @brief Obtain unit as human-readable string
         * @return Unit of the metric
         */
        std::string_view unit() const { return unit_; }

        /**
         * @brief Obtain type of the metric
         * @return Metric type
         */
        MetricType type() const { return type_; }

    private:
        std::string name_;
        std::string unit_;
        MetricType type_;
    };

    /**
     * @brief This class represents a timed metric for telemetry or data quality monitoring
     *
     * @details A timed metric is a metric that is polled in regular intervals. It requires an interval, a value callback,
     *          and a list of states where the callback is allowed to be called.
     */
    class TimedMetric : public Metric {
    public:
        /**
         * @brief Timed metric constructor
         *
         * @param name Name of the metric
         * @param unit Unit of the metric as human readable string
         * @param type Type of the metric
         * @param interval Interval in which to send the metric
         * @param value_callback Callback to determine the current value of the metric
         */
        TimedMetric(std::string name,
                    std::string unit,
                    MetricType type,
                    std::chrono::nanoseconds interval,
                    std::function<std::optional<config::Value>()> value_callback)
            : Metric(std::move(name), std::move(unit), type), interval_(interval),
              value_callback_(std::move(value_callback)) {}

        /**
         * @brief Obtain interval of the metric
         * @return Interval in nanoseconds
         */
        std::chrono::nanoseconds interval() const { return interval_; }

        /**
         * @brief Evaluate the value callback to get the current value of the metric
         * @return Optional with current value or no value
         */
        std::optional<config::Value> currentValue() { return value_callback_(); }

    private:
        std::chrono::nanoseconds interval_;
        std::function<std::optional<config::Value>()> value_callback_;
    };

    /**
     * @brief Class containing a pointer to a metric and a corresponding metric value
     */
    class MetricValue {
    public:
        /**
         * @brief Metric value constructor
         *
         * @param metric Shared pointer to a metric
         * @param value Value corresponding to the metric
         */
        MetricValue(std::shared_ptr<Metric> metric, config::Value&& value)
            : metric_(std::move(metric)), value_(std::move(value)) {}

        MetricValue() = default;

        /**
         * @brief Obtain the underlying metric
         * @return Shared pointer tot the metric
         */
        std::shared_ptr<Metric> getMetric() const { return metric_; }

        /**
         * @brief Obtain the metric value
         * @return Value of the metric
         */
        const config::Value& getValue() const { return value_; }

        /** Assemble metric via msgpack for message payload */
        CNSTLN_API message::PayloadBuffer assemble() const;

        /** Disassemble metric from message payload */
        CNSTLN_API static MetricValue disassemble(std::string name, const message::PayloadBuffer& message);

    private:
        std::shared_ptr<Metric> metric_;
        config::Value value_;
    };

} // namespace constellation::metrics
