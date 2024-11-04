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
#include <string_view>
#include <utility>

#include "constellation/build.hpp"

namespace constellation::exec {

    struct SatelliteType {
        SatelliteType(std::string _type_name, std::filesystem::path _dso_path = {})
            : type_name(std::move(_type_name)), dso_path(std::move(_dso_path)) {}

        /** Name of satellite type */
        std::string type_name;

        /** Path to the Dynamic Shared Object (DSO) that contains the satellite */
        std::filesystem::path dso_path;
    };

    /**
     * Provides the main function for a satellite
     *
     * @param argc CLI argument count
     * @param argv CLI arguments
     * @param program Name of the CLI executable
     * @param satellite_type Optional satellite type to pre-load
     */
    CNSTLN_API int satellite_main(int argc,
                                  char* argv[], // NOLINT(modernize-avoid-c-arrays)
                                  std::string_view program,
                                  std::optional<SatelliteType> satellite_type = std::nullopt) noexcept;

    // Handler for signal like SIGINT etc
    extern "C" CNSTLN_API void signal_hander(int signal);

} // namespace constellation::exec
