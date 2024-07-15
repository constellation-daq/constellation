/**
 * @file
 * @brief Implementation of metric classes
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Metric.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

#include <magic_enum.hpp>
#include <msgpack.hpp>

#include "constellation/core/config/Value.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/std_future.hpp"

using namespace constellation::metrics;
using namespace constellation::message;
using namespace constellation::utils;

PayloadBuffer Metric::assemble() const {
    msgpack::sbuffer sbuf {};
    msgpack::pack(sbuf, this->value_);
    msgpack::pack(sbuf, std::to_underlying(this->type()));
    msgpack::pack(sbuf, this->unit());
    return {std::move(sbuf)};
}

Metric Metric::disassemble(const message::PayloadBuffer& message) {
    // Offset since we decode four separate msgpack objects
    std::size_t offset = 0;

    // Unpack value
    const auto msgpack_value = msgpack::unpack(to_char_ptr(message.span().data()), message.span().size(), offset);
    auto value = msgpack_value->as<config::Value>();

    // Unpack type
    const auto msgpack_type = msgpack::unpack(to_char_ptr(message.span().data()), message.span().size(), offset);
    const auto type = magic_enum::enum_cast<metrics::Type>(msgpack_type->as<std::uint8_t>());

    // Unpack unit
    const auto msgpack_unit = msgpack::unpack(to_char_ptr(message.span().data()), message.span().size(), offset);
    const auto unit = msgpack_unit->as<std::string>();

    if(!type.has_value()) {
        throw std::invalid_argument("Invalid metric type");
    }

    return {unit, type.value(), std::move(value)};
}

void MetricTimer::update(const config::Value& value) {
    set(value);
    changed_ = true;
}

bool MetricTimer::check(State state) {

    // First check the metric condition to update internals
    if(!condition()) {
        return false;
    }

    // If the metric has not been changed, there is no need to send it again
    if(!changed_) {
        return false;
    }

    // Check if we are supposed to distribute this metric from the current state:
    // Note: empty state list means that it is always distributed.
    if(!states_.empty() && !states_.contains(state)) {
        return false;
    }

    // All checks passed, send metric
    changed_ = false;
    return true;
}

bool TimedMetric::condition() {

    auto duration = Clock::now() - last_trigger_;

    if(duration >= interval_) {
        last_trigger_ += interval_;
        return true;
    }

    last_check_ = Clock::now();
    return false;
}

Clock::time_point TimedMetric::nextTrigger() const {
    return last_check_ + interval_;
}

TriggeredMetric::TriggeredMetric(
    std::string unit, Type type, std::size_t triggers, std::initializer_list<State> states, config::Value&& initial_value)
    : MetricTimer(std::move(unit), type, states, std::move(initial_value)), triggers_(triggers) {
    // We have an initial value, let's log it directly
    if(!std::holds_alternative<std::monostate>(initial_value)) {
        current_triggers_ = triggers_;
    }
}

bool TimedAutoMetric::condition() {
    auto expired = TimedMetric::condition();

    if(expired) {
        // Update the metrics value from the function
        update(func_());
    }
    return expired;
}

void TriggeredMetric::update(const config::Value& value) {
    MetricTimer::update(value);
    current_triggers_++;
}

bool TriggeredMetric::condition() {

    if(current_triggers_ >= triggers_) {
        current_triggers_ = 0;
        return true;
    }

    return false;
}
