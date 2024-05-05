/**
 * @file
 * @brief Satellite exceptions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "constellation/core/message/satellite_definitions.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/core/utils/type.hpp"

namespace constellation::satellite {

    /**
     * @ingroup Exceptions
     * @brief Finite State Machine Error
     *
     * An error occurred in a request to the finite state machine
     */
    class FSMError : public utils::RuntimeError {
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
    class InvalidFSMTransition : public FSMError {
    public:
        explicit InvalidFSMTransition(const message::Transition transition, const message::State state) {
            error_message_ = "Transition ";
            error_message_ += utils::to_string(transition);
            error_message_ += " not allowed from ";
            error_message_ += utils::to_string(state);
            error_message_ += " state";
        }
    };

    /** Error thrown for all user command errors */
    class UserCommandError : public utils::RuntimeError {
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
    class UnknownUserCommand : public UserCommandError {
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
    class InvalidUserCommand : public UserCommandError {
    public:
        explicit InvalidUserCommand(const std::string& command, const message::State state) {
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
    class MissingUserCommandArguments : public UserCommandError {
    public:
        explicit MissingUserCommandArguments(const std::string& command, size_t args_expected, size_t args_given) {
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
    class InvalidUserCommandArguments : public UserCommandError {
    public:
        explicit InvalidUserCommandArguments(const std::type_info& argtype, const std::type_info& valuetype) {
            error_message_ = "Mismatch of argument type \"";
            error_message_ += utils::demangle(argtype);
            error_message_ += "\" to provided type \"";
            error_message_ += utils::demangle(valuetype);
            error_message_ += "\"";
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Invalid return type from user command
     */
    class InvalidUserCommandResult : public UserCommandError {
    public:
        explicit InvalidUserCommandResult(const std::type_info& argtype) {
            error_message_ = "Error casting function return type \"";
            error_message_ += utils::demangle(argtype);
            error_message_ += "\" to dictionary value";
        }
    };

} // namespace constellation::satellite
