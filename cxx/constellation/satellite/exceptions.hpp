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
#include <cstddef>
#include <string>
#include <string_view>

#include "constellation/build.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/string.hpp"

namespace constellation::satellite {

    /**
     * @ingroup Exceptions
     * @brief Generic Satellite Error
     *
     * An unspecified error occurred in the user code implementation of a satellite
     */
    class CNSTLN_API SatelliteError : public utils::RuntimeError {
    public:
        explicit SatelliteError(const std::string& reason) { error_message_ = reason; }

    protected:
        SatelliteError() = default;
    };

    /**
     * @ingroup Exceptions
     * @brief Satellite Error for device communication
     *
     * An error occurred in the user code implementation of a satellite when attempting to communicate with hardware
     */
    class CNSTLN_API CommunicationError : public SatelliteError {
    public:
        explicit CommunicationError(const std::string& reason) { error_message_ = reason; }
    };

    /**
     * @ingroup Exceptions
     * @brief Finite State Machine Error
     *
     * An error occurred in a request to the finite state machine
     */
    class CNSTLN_API FSMError : public utils::RuntimeError {
        explicit FSMError(const std::string& reason) { error_message_ = reason; }

    protected:
        FSMError() = default;
    };

    /**
     * @ingroup Exceptions
     * @brief Invalid transition requested
     *
     * A transition of the finite state machine was requested which is not allowed from the current state
     */
    class CNSTLN_API InvalidFSMTransition : public FSMError {
    public:
        explicit InvalidFSMTransition(protocol::CSCP::Transition transition, protocol::CSCP::State state) {
            error_message_ = "Transition ";
            error_message_ += utils::to_string(transition);
            error_message_ += " not allowed from ";
            error_message_ += utils::to_string(state);
            error_message_ += " state";
        }
    };

    /** Error thrown for all user command errors */
    class CNSTLN_API UserCommandError : public utils::RuntimeError {
        explicit UserCommandError(const std::string& reason) { error_message_ = reason; }

    protected:
        UserCommandError() = default;
    };

    /**
     * @ingroup Exceptions
     * @brief Invalid user command
     *
     * The user command is not registered
     */
    class CNSTLN_API UnknownUserCommand : public UserCommandError {
    public:
        explicit UnknownUserCommand(const std::string& command) {
            error_message_ = "Unknown command \"";
            error_message_ += command;
            error_message_ += "\"";
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Invalid user command
     *
     * The user command is not valid in the current state of the finite state machine
     */
    class CNSTLN_API InvalidUserCommand : public UserCommandError {
    public:
        explicit InvalidUserCommand(const std::string& command, protocol::CSCP::State state) {
            error_message_ = "Command ";
            error_message_ += command;
            error_message_ += " cannot be called in state ";
            error_message_ += utils::to_string(state);
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Missing arguments for user command
     */
    class CNSTLN_API MissingUserCommandArguments : public UserCommandError {
    public:
        explicit MissingUserCommandArguments(const std::string& command, std::size_t args_expected, std::size_t args_given) {
            error_message_ = "Command \"";
            error_message_ += command;
            error_message_ += "\" expects ";
            error_message_ += utils::to_string(args_expected);
            error_message_ += " arguments but ";
            error_message_ += utils::to_string(args_given);
            error_message_ += " given";
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Invalid arguments for user command
     */
    class CNSTLN_API InvalidUserCommandArguments : public UserCommandError {
    public:
        explicit InvalidUserCommandArguments(std::string_view argtype, std::string_view valuetype) {
            error_message_ = "Mismatch of argument type \"";
            error_message_ += argtype;
            error_message_ += "\" to provided type \"";
            error_message_ += valuetype;
            error_message_ += "\"";
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Invalid return type from user command
     */
    class CNSTLN_API InvalidUserCommandResult : public UserCommandError {
    public:
        explicit InvalidUserCommandResult(std::string_view argtype) {
            error_message_ = "Error casting function return type \"";
            error_message_ += argtype;
            error_message_ += "\" to dictionary value";
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Error when sending a message timed out
     */
    class CNSTLN_API SendTimeoutError : public SatelliteError {
    public:
        explicit SendTimeoutError(const std::string& what, std::chrono::seconds timeout) {
            error_message_ = "Failed sending " + what + " after " + utils::to_string<std::chrono::seconds>(timeout);
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Error when receiving a message timed out
     */
    class CNSTLN_API RecvTimeoutError : public satellite::SatelliteError {
    public:
        explicit RecvTimeoutError(const std::string& what, std::chrono::seconds timeout) {
            error_message_ = "Failed receiving " + what + " after " + utils::to_string<std::chrono::seconds>(timeout);
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Error when a received CDTP message does not have the correct type
     */
    class CNSTLN_API InvalidCDTPMessageType : public satellite::SatelliteError {
    public:
        explicit InvalidCDTPMessageType(message::CDTP1Message::Type type, std::string_view reason) {
            error_message_ = "Error handling CDTP message with type " + utils::to_string(type) + ": ";
            error_message_ += reason;
        }
    };

} // namespace constellation::satellite
