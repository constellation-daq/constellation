/**
 * @file
 * @brief Collection of all configuration exceptions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <initializer_list>
#include <string>
#include <typeinfo>

#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/type.hpp"

namespace constellation::config {
    /**
     * @ingroup Exceptions
     * @brief Base class for all configurations exceptions in the framework.
     */
    class ConfigurationError : public utils::RuntimeError {};

    /**
     * @ingroup Exceptions
     * @brief Informs of a missing key that should have been defined
     */
    class MissingKeyError : public ConfigurationError {
    public:
        /**
         * @brief Construct an error for a missing key
         * @param key Name of the missing key
         */
        MissingKeyError(const std::string& key) { error_message_ = "Key '" + key + "' does not exist"; }
    };

    /**
     * @ingroup Exceptions
     * @brief Indicates a problem converting the value of a configuration key to the value it should represent
     */
    class InvalidTypeError : public ConfigurationError {
    public:
        /**
         * @brief Construct an error for a value with an invalid type
         * @param key Name of the corresponding key
         * @param vtype Type of the stored value
         * @param type Type the value should have been converted to
         * @param reason Reason why the conversion failed
         */
        InvalidTypeError(const std::string& key,
                         const std::type_info& vtype,
                         const std::type_info& type,
                         const std::string& reason = "") {
            // FIXME wording
            error_message_ = "Could not convert value of type '";
            error_message_ += utils::demangle(vtype);
            error_message_ += "' to type '";
            error_message_ += utils::demangle(type);
            error_message_ += "' for key '" + key + "'";
            if(!reason.empty()) {
                error_message_ += ": " + reason;
            }
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Indicates an error with the contents of value
     *
     * Should be raised if the data contains valid data for its type (otherwise an \ref InvalidTypeError should have been
     * raised earlier), but the value is not in the range of allowed values.
     */
    class InvalidValueError : public ConfigurationError {
    public:
        /**
         * @brief Construct an error for an invalid value
         * @param value invalid value in string representation
         * @param key Name of the problematic key
         * @param reason Reason why the value is invalid (empty if no explicit reason)
         */
        InvalidValueError(const std::string& value, const std::string& key, const std::string& reason = "") {
            error_message_ = "Value " + value + " of key '" + key + "' is not valid";
            if(!reason.empty()) {
                error_message_ += ": " + reason;
            }
        }
    };

    // Forward declaration of Configuration class
    class Configuration;

    /**
     * @ingroup Exceptions
     * @brief Indicates an error with a combination of configuration keys
     *
     * Should be raised if a disallowed combination of keys is used, such as two optional parameters which cannot be used at
     * the same time because they contradict each other.
     */
    class InvalidCombinationError : public ConfigurationError {
    public:
        /**
         * @brief Construct an error for an invalid combination of keys
         * @param config Configuration object containing the problematic key combination
         * @param keys List of names of the conflicting keys
         * @param reason Reason why the key combination is invalid (empty if no explicit reason)
         */
        InvalidCombinationError(const Configuration& config,
                                std::initializer_list<std::string> keys,
                                const std::string& reason = "");
    };

} // namespace constellation::config
