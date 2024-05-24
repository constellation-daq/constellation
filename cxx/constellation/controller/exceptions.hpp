/**
 * @file
 * @brief Collection of all controller exceptions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <sstream>
#include <string>
#include <string_view>

#include <toml++/toml.hpp>

#include "constellation/core/utils/exceptions.hpp"

namespace constellation::controller {
    /**
     * @ingroup Exceptions
     * @brief Base class for all controller exceptions in the framework.
     */
    class ControllerError : public utils::RuntimeError {};

    /**
     * @ingroup Exceptions
     * @brief Informs of a problem in parsing a configuration
     */
    class ConfigParseError : public ControllerError {
    public:
        /**
         * @brief Construct an error for a missing key
         * @param key Name of the missing key
         */
        ConfigParseError(const toml::source_region& source, std::string_view issue) {
            error_message_ = "Error parsing file \"";
            std::stringstream s;
            s << source.path << "\" at position " << source.begin;
            error_message_ += s.str();
            error_message_ += ": ";
            error_message_ = issue;
        }
    };

} // namespace constellation::controller
