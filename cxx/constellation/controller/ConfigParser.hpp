/**
 * @file
 * @brief Configuration file parser
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include "constellation/core/config/Configuration.hpp"

namespace constellation::controller {

    /**
     * @brief Reader class for configuration files
     *
     * Read TOML configuration file and provide access methods to obtain individual satellite configurations
     */
    class CNSTLN_API ConfigParser {
    public:
        /**
         * @brief Constructs a config parser from a file
         * @param file Name of the file related to the stream
         */
        explicit ConfigParser(std::filesystem::path file);

        /**
         * @brief Check if a configuration exists
         * @param name Name of a configuration header to search for
         * @return True if at least a single configuration with this name exists, false otherwise
         */
        bool hasConfiguration(std::string_view name) const;
        /**
         * @brief Count the number of configurations with a particular name
         * @param name Name of a configuration header
         * @return The number of configurations with the given name
         */
        unsigned int countConfigurations(std::string_view name) const;

        /**
         * @brief Get combined configuration of all empty sections (usually the header)
         * @note Typically this is only the section at the top of the file
         * @return Configuration object for the empty section
         */
        config::Configuration getHeaderConfiguration() const;

        /**
         * @brief Get all configurations with a particular header
         * @param name Header name of the configurations to return
         * @return List of configurations with the given name
         */
        std::vector<config::Configuration> getConfigurations(std::string_view name) const;

        /**
         * @brief Get all configurations
         * @return List of all configurations
         */
        std::vector<config::Configuration> getConfigurations() const;
    };
} // namespace constellation::controller
