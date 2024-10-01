/**
 * @file
 * @brief Implementation of configuration exceptions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "exceptions.hpp"

#include <initializer_list>
#include <string>

#include "constellation/core/config/Configuration.hpp"

using namespace constellation::config;

InvalidCombinationError::InvalidCombinationError(const Configuration& config,
                                                 std::initializer_list<std::string> keys,
                                                 const std::string& reason) {
    error_message_ = "Combination of keys ";
    for(const auto& key : keys) {
        if(!config.has(key)) {
            continue;
        }
        error_message_ += "'" + key + "', ";
    }
    error_message_ += " is not valid";
    if(!reason.empty()) {
        error_message_ += ": " + reason;
    }
}

InvalidValueError::InvalidValueError(const Configuration& config, const std::string& key, const std::string& reason) {
    error_message_ = "Value " + config.getText(key) + " of key '" + key + "' is not valid";
    if(!reason.empty()) {
        error_message_ += ": " + reason;
    }
}

InvalidValueError::InvalidValueError(const std::string& value, const std::string& key, const std::string& reason) {
    error_message_ = "Value " + value + " of key '" + key + "' is not valid";
    if(!reason.empty()) {
        error_message_ += ": " + reason;
    }
}
