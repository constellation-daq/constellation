/**
 * @file
 * @brief Main function for a satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "constellation/core/config.hpp"

namespace constellation::exec {

    struct SatelliteClass {
        SatelliteClass(std::string _class_name, std::filesystem::path _dso_path = {})
            : class_name(std::move(_class_name)), dso_path(std::move(_dso_path)) {}

        /** Name of satellite class */
        std::string class_name;

        /** Path to the Dynamic Shared Object (DSO) that contains the satellite */
        std::filesystem::path dso_path {};
    };

    /**
     * Provides the main function for a satellite
     *
     * @param argc CLI argument count
     * @param argv CLI arguments
     * @param program Name of the CLI executable
     * @param satellite_class Optional satellite class to pre-load
     */
    CNSTLN_API int satellite_main(int argc,
                                  char* argv[], // NOLINT(*-avoid-c-arrays)
                                  std::string program,
                                  std::optional<SatelliteClass> satellite_class = std::nullopt) noexcept;

} // namespace constellation::exec
