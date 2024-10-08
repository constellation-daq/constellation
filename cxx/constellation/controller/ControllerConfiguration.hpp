/**
 * @file
 * @brief Configuration parser class
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <filesystem>
#include <string_view>

#include "constellation/build.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/utils/string_hash_map.hpp"

namespace constellation::controller {

    /**
     * @brief Configuration parser to read TOML files and emit dictionaries for individual satellites
     *
     * The configuration file holds a hierarchy of tables which contain the configuration keys for all satellites of the
     * Constellation. The dictionaries for the individual satellites need to be assembled from keys specific to the
     * respective satellite, keys valid for the relevant satellite type and keys intended for all satellites.
     */
    class ControllerConfiguration {
    public:
        CNSTLN_API virtual ~ControllerConfiguration() = default;

        /**
         * @brief Default constructor with empty configuration dictionaries
         */
        ControllerConfiguration() = default;

        /**
         * @brief Construct a controller configuration and parse dictionaries from a string
         *
         * @param toml TOML data as string
         *
         * @throws ConfigFileNotFoundError if the configuration file could not be found or opened
         * @throws ConfigFileParseError if the configuration file could not be parsed into valid TOML
         * @throws ConfigFileTypeError if the configuration file contained invalid value types
         */
        CNSTLN_API ControllerConfiguration(std::string_view toml);

        /**
         * @brief Construct a controller configuration and parse dictionaries from a configuration file
         *
         * @param path File path to the TOML configuration file
         *
         * @throws ConfigFileNotFoundError if the configuration file could not be found or opened
         * @throws ConfigFileParseError if the configuration file could not be parsed into valid TOML
         * @throws ConfigFileTypeError if the configuration file contained invalid value types
         */
        CNSTLN_API explicit ControllerConfiguration(const std::filesystem::path& path);

        /// @cond doxygen_suppress
        ControllerConfiguration(const ControllerConfiguration& other) = delete;
        ControllerConfiguration& operator=(const ControllerConfiguration& other) = delete;
        ControllerConfiguration(ControllerConfiguration&& other) noexcept = delete;
        ControllerConfiguration& operator=(ControllerConfiguration&& other) = delete;
        /// @endcond

        /**
         * @brief Check if a configuration exists for a given satellite
         *
         * This only checks the specific (named) satellite sections of the configuration file, not any type-bound or global
         * configuration key-value-pairs.
         *
         * @return True if a configuration is available for the satellite with the given name, false otherwise
         */
        CNSTLN_API bool hasSatelliteConfiguration(std::string_view canonical_name) const;

        /**
         * @brief Prepare and return configuration dictionary for a given satellite
         *
         * The cached dictionaries from parsed from the input TOML are searched for the given satellite, and keys from the
         * type section matching this satellite as well as global keys to all satellites are added. Name and type are matched
         * case-insensitively.
         *
         * @return Configuration dictionary, possibly empty if the satellite was not found in the cached configuration
         */
        CNSTLN_API config::Dictionary getSatelliteConfiguration(std::string_view canonical_name) const;

    private:
        /**
         * @brief Parse a string view with TOML data into dictionaries
         *
         * @param toml TOML data as string
         *
         * @throws ConfigFileNotFoundError if the configuration file could not be found or opened
         * @throws ConfigFileParseError if the configuration file could not be parsed into valid TOML
         * @throws ConfigFileTypeError if the configuration file contained invalid value types
         */
        void parse_toml(std::string_view toml);

    private:
        /* Key-value pairs of the global satellite section */
        config::Dictionary global_config_;

        /* Dictionaries of satellite type sections */
        utils::string_hash_map<config::Dictionary> type_configs_;

        /**
         * Dictionaries for individual satellites
         *
         * @note The keys here are the full canonical names of the satellites since the same name for different-type
         *       satellites are allowed
         */
        utils::string_hash_map<config::Dictionary> satellite_configs_;

        /* Logger */
        log::Logger config_parser_logger_ {"CTRLCFG"};
    };

} // namespace constellation::controller
