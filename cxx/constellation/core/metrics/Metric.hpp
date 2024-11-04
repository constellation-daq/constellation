/**
 * @file
 * @brief Metric class
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "constellation/build.hpp"
#include "constellation/core/config/Value.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"

namespace constellation::metrics {

    /** Metrics types */
    enum class Type : std::uint8_t {
        /** Always keep the latest value, replace earlier ones */
        LAST_VALUE = 1,
        /** Sum every new value to previously received ones */
        ACCUMULATE = 2,
        /** Calculate the average value */
        AVERAGE = 3,
        /** Calculate the rate from the value over a given time interval */
        RATE = 4,
    };
    using enum Type;

    /**
     * @class Metric
     * @brief This class represents a metric for data quality monitoring or statistics purposes.
     *
     *  @details It comprises a value, a unit and a type. The type defines how the value should be treated, i.e. if always
     *  the last transmitted value should be displayed, if an average of the values should be calculated or if they should
     *  be accumulated.
     */
    class CNSTLN_API Metric {
    public:
        /**
         * @brief Metrics constructor
         *
         * @param unit Unit of this metric as human readable string
         * @param type Type of the metric
         * @param initial_value Initial value
         */
        Metric(std::string unit, Type type, config::Value&& initial_value)
            : unit_(std::move(unit)), type_(type), value_(std::move(initial_value)) {}

        Metric() : type_(LAST_VALUE), value_(std::monostate {}) {};
        virtual ~Metric() noexcept = default;

        // Default copy/move constructor/assignment
        Metric(const Metric&) = default;
        Metric& operator=(const Metric&) = default;
        Metric(Metric&&) noexcept = default;
        Metric& operator=(Metric&&) = default;

        /** Setting or updating the value */
        void set(config::Value&& value) { value_ = std::move(value); }

        /**
         * @brief Obtain current metric value
         * @return Current value
         */
        template <typename T> T value() const { return value_.get<T>(); }

        /**
         * @brief Obtain unit as human-readable string
         * @return Unit of the metric
         */
        std::string_view unit() const { return unit_; }

        /**
         * @brief Obtain type of the metric
         * @return Metric type
         */
        Type type() const { return type_; }

        /** Assemble metric via msgpack for message payload */
        message::PayloadBuffer assemble() const;

        /** Disassemble metric from message payload */
        static Metric disassemble(const message::PayloadBuffer& message);

    private:
        std::string unit_;
        Type type_;
        config::Value value_;
    };
} // namespace constellation::metrics
