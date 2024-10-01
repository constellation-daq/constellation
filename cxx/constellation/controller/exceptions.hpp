/**
 * @file
 * @brief Controller & ConfigParser exceptions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <filesystem>
#include <string_view>

#include "constellation/build.hpp"
#include "constellation/core/utils/exceptions.hpp"

namespace constellation::controller {
    /**
     * @ingroup Exceptions
     * @brief Base class for all controller exceptions
     */
    class CNSTLN_API ControllerError : public utils::RuntimeError {};

    /**
     * @ingroup Exceptions
     * @brief Notifies of a missing configuration file
     */
    class CNSTLN_API ConfigFileNotFoundError : public ControllerError {
    public:
        /**
         * @brief Construct an error for a configuration that is not found
         * @param file_name Name of the configuration file
         */
        explicit ConfigFileNotFoundError(const std::filesystem::path& file_name) {
            error_message_ = "Could not read configuration file " + file_name.string();
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Error with parsing the file content to TOML
     */
    class CNSTLN_API ConfigFileParseError : public ControllerError {
    public:
        /**
         * @brief Construct an error for a configuration file that cannot correctly be parsed as TOML
         * @param error Error message returned by the TOML parser
         */
        explicit ConfigFileParseError(std::string_view error) {
            error_message_ = "Could not parse content of configuration file: ";
            error_message_ += error;
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Error with the type of a key in the configuration file
     */
    class CNSTLN_API ConfigFileTypeError : public ControllerError {
    public:
        /**
         * @brief Construct an error for a configuration key that has an invalid type
         * @param key The offending configuration key
         * @param error Error message
         */
        explicit ConfigFileTypeError(std::string_view key, std::string_view error) {
            error_message_ = "Invalid value type for key ";
            error_message_ += key;
            error_message_ += ": ";
            error_message_ += error;
        }
    };
} // namespace constellation::controller
