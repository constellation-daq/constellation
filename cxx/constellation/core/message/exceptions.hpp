/**
 * @file
 * @brief Collection of all message exceptions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <string_view>

#include "constellation/build.hpp"
#include "constellation/core/message/Protocol.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/string.hpp"

namespace constellation::message {
    /**
     * @ingroup Exceptions
     * @brief Error in decoding the message
     *
     * The message cannot be correctly decoded because the format does not adhere to protocol.
     */
    class CNSTLN_API MessageDecodingError : public utils::RuntimeError {
    public:
        explicit MessageDecodingError(std::string_view reason) {
            error_message_ = "Error decoding message: ";
            error_message_ += reason;
        }

    protected:
        MessageDecodingError() = default;
    };

    /**
     * @ingroup Exceptions
     * @brief Invalid protocol identifier
     *
     * The message identifier does not represent a valid or known protocol identifier
     */
    class CNSTLN_API InvalidProtocolError : public MessageDecodingError {
    public:
        explicit InvalidProtocolError(std::string_view protocol_identifier) {
            error_message_ = "Invalid protocol identifier \"";
            error_message_ += protocol_identifier;
            error_message_ += "\"";
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Unexpected protocol identifier
     *
     * The protocol identifier of the message does not match the expected protocol
     */
    class CNSTLN_API UnexpectedProtocolError : public MessageDecodingError {
    public:
        explicit UnexpectedProtocolError(Protocol prot_recv, Protocol prot_exp) {
            error_message_ = "Received protocol \"";
            error_message_ += utils::to_string(prot_recv);
            error_message_ += "\" does not match expected protocol \"";
            error_message_ += utils::to_string(prot_exp);
            error_message_ += "\"";
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Incorrect message type
     *
     * The message type does not match the requested operation
     */
    class CNSTLN_API IncorrectMessageType : public utils::RuntimeError {
    public:
        explicit IncorrectMessageType(std::string_view why) {
            error_message_ = "Message type is incorrect: ";
            error_message_ += why;
        }
    };

} // namespace constellation::message
