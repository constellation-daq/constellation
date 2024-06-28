/**
 * @file
 * @brief Command dispatcher for user commands
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "CommandRegistry.hpp"

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "constellation/core/config/Value.hpp"
#include "constellation/core/message/satellite_definitions.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/exceptions.hpp"
#include "constellation/satellite/fsm_definitions.hpp"

namespace constellation::satellite {

    template <typename T> inline T CommandRegistry::to_argument(const config::Value& value) {
        try {
            return value.get<T>();
        } catch(const std::bad_variant_access&) {
            throw InvalidUserCommandArguments(typeid(T), value.type());
        }
    }

    template <typename T> inline config::Value CommandRegistry::convert(const T& value) {
        try {
            return config::Value::set(value);
        } catch(const std::bad_cast&) {
            throw InvalidUserCommandResult(typeid(T));
        }
    }

    template <typename R, typename... Args>
    inline void CommandRegistry::add(const std::string& name,
                                     std::string description,
                                     std::initializer_list<State> states,
                                     std::function<R(Args...)> func) {
        const auto name_lc = utils::transform(name, ::tolower);
        if(!message::is_valid_command_name(name_lc)) {
            throw utils::LogicError("Command name is invalid");
        }

        if(magic_enum::enum_cast<message::StandardCommand>(name_lc).has_value()) {
            throw utils::LogicError("Standard satellite command with this name exists");
        }

        if(magic_enum::enum_cast<message::TransitionCommand>(name_lc).has_value()) {
            throw utils::LogicError("Satellite transition command with this name exists");
        }

        const auto [it, success] = commands_.emplace(
            name_lc, Command {generate_call(std::move(func)), sizeof...(Args), std::move(description), states});

        if(!success) {
            throw utils::LogicError("Command \"" + name_lc + "\" is already registered");
        }
    }

    template <typename T, typename R, typename... Args>
    inline void CommandRegistry::add(const std::string& name,
                                     std::string description,
                                     std::initializer_list<State> states,
                                     R (T::*func)(Args...),
                                     T* t) {
        if(!func || !t) {
            throw utils::LogicError("Object and member function pointers must not be nullptr");
        }
        add(name, std::move(description), states, std::function<R(Args...)>([=](Args... args) {
                return (t->*func)(args...);
            }));
    }

} // namespace constellation::satellite
