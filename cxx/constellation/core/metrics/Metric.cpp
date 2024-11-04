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
