/**
 * @file
 * @brief Loader for functions from Dynamic Shared Object (DSO)
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <filesystem>
#include <string>

#include "constellation/core/config.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/satellite/Satellite.hpp"

namespace constellation::exec {
    class DSOLoader {
    public:
        /**
         * @brief Create a class to load a shared library
         * @param dso_name Name of the DSO without prefix or file extension
         * @param logger Logger to log DSO path on success
         * @param hint Hint to path of the DSO
         */
        CNSTLN_API DSOLoader(std::string dso_name, log::Logger& logger, std::filesystem::path hint = {});

        CNSTLN_API virtual ~DSOLoader();

        // No copy/move constructor/assignment
        DSOLoader(DSOLoader& other) = delete;
        DSOLoader& operator=(DSOLoader other) = delete;
        DSOLoader(DSOLoader&& other) = delete;
        DSOLoader& operator=(DSOLoader&& other) = delete;

        /**
         * @brief Returns function pointer to function from DSO
         * @tparam FunctionType the typedef of the function
         * @param function_name the name of the function
         */
        template <typename FunctionType> FunctionType* getFunctionFromDSO(const std::string& function_name) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            return reinterpret_cast<FunctionType*>(getRawFunctionFromDSO(function_name));
        }

        /**
         * @brief Load the satellite generator from the DSO
         */
        CNSTLN_API satellite::Generator* loadSatelliteGenerator();

    private:
        // OS-specific function to get function from DSO
        CNSTLN_API void* getRawFunctionFromDSO(const std::string& function_name);

        std::string dso_name_;
        void* handle_;
    };
} // namespace constellation::exec
