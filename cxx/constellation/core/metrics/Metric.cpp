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
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <msgpack.hpp>

#include "constellation/core/config/Value.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/msgpack.hpp"
#include "constellation/core/utils/std_future.hpp" // IWYU pragma: keep

using namespace constellation::metrics;
using namespace constellation::message;
using namespace constellation::utils;

PayloadBuffer MetricValue::assemble() const {
    msgpack::sbuffer sbuf {};
    msgpack_pack(sbuf, value_);
    msgpack_pack(sbuf, std::to_underlying(metric_->type()));
    msgpack_pack(sbuf, metric_->unit());
    return {std::move(sbuf)};
}

MetricValue MetricValue::disassemble(std::string name, const message::PayloadBuffer& message) {
    // Offset since we decode separate msgpack objects
    std::size_t offset = 0;

    try {
        // Unpack value
        auto value = msgpack_unpack_to<config::Value>(to_char_ptr(message.span().data()), message.span().size(), offset);

        // Unpack type
        const auto type = enum_cast<MetricType>(
            msgpack_unpack_to<std::uint8_t>(to_char_ptr(message.span().data()), message.span().size(), offset));

        // Unpack unit
        const auto unit = msgpack_unpack_to<std::string>(to_char_ptr(message.span().data()), message.span().size(), offset);

        if(!type.has_value()) {
            throw std::invalid_argument("Invalid metric type");
        }

        return {std::make_shared<Metric>(std::move(name), unit, type.value()), std::move(value)};
    } catch(const MsgPackError& e) {
        throw std::invalid_argument(e.what());
    }
}
