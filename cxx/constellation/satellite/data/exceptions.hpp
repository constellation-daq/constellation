/**
 * @file
 * @brief Satellite exceptions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>

#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/exceptions.hpp"

namespace constellation::data {

    /**
     * @ingroup Exceptions
     * @brief Error sending a data message
     */
    class SendTimeoutError : public satellite::SatelliteError {
    public:
        explicit SendTimeoutError(const std::string& what, std::chrono::seconds timeout) {
            error_message_ = "Failed sending " + what + " after " + utils::to_string<std::chrono::seconds>(timeout);
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Error receiving a data message
     */
    class RecvTimeoutError : public satellite::SatelliteError {
    public:
        explicit RecvTimeoutError(const std::string& what, std::chrono::seconds timeout) {
            error_message_ = "Failed receiving " + what + " after " + utils::to_string<std::chrono::seconds>(timeout);
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Invalid state in DataSender/DataReceiver
     *
     * This appears when the interface of the DataSender/DataReceiver is used incorrectly
     */
    class InvalidDataState : public utils::LogicError {
    public:
        explicit InvalidDataState(const std::string& action, const std::string& state) {
            error_message_ = "Cannot perform " + action + " in data state " + state;
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Invalid message type
     */
    class InvalidMessageType : public utils::LogicError {
    public:
        explicit InvalidMessageType(message::CDTP1Message::Type type, message::CDTP1Message::Type expected_type) {
            error_message_ =
                "Expected CDTP message type " + utils::to_string(expected_type) + " but received " + utils::to_string(type);
        }
    };

} // namespace constellation::data
