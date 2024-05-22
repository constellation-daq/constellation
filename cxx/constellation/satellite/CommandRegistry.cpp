/**
 * @file
 * @brief Implementation of the Command Dispatcher
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "CommandRegistry.hpp"

#include <map>
#include <numeric>
#include <string>
#include <vector>

#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/config/Value.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/fsm_definitions.hpp"

using namespace constellation;
using namespace constellation::satellite;
using namespace constellation::utils;

config::Value CommandRegistry::call(State state, const std::string& name, const config::List& args) {
    const auto cmd = commands_.find(name);

    // Check if this is a known command at all
    if(cmd == commands_.end()) {
        throw UnknownUserCommand(name);
    }

    // Check if we are allowed to call this command from the current state:
    // Note: empty state list means that everything is allowed.
    if(!cmd->second.valid_states.empty() && !cmd->second.valid_states.contains(state)) {
        throw InvalidUserCommand(name, state);
    }

    // Check if all required arguments are present:
    if(args.size() != cmd->second.nargs) {
        throw MissingUserCommandArguments(name, cmd->second.nargs, args.size());
    }

    // Call the command:
    return cmd->second.func(args);
}

std::map<std::string, std::string> CommandRegistry::describeCommands() const {
    std::map<std::string, std::string> cmds {};

    // Add all commands tot he map
    for(const auto& cmd : commands_) {
        auto description = cmd.second.description;

        // Augment description with number of required parameters
        description += "\nThis command requires ";
        description += to_string(cmd.second.nargs);
        description += " arguments.";

        // Append allowed states (empty means allowed from all states)
        if(!cmd.second.valid_states.empty()) {
            description += "\nThis command can only be called in the following states: ";
            description += range_to_string(cmd.second.valid_states);
        } else {
            description += "\nThis command can be called in all states.";
        }

        cmds.emplace(cmd.first, description);
    }

    return cmds;
}
