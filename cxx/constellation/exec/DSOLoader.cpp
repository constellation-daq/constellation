/**
 * @file
 * @brief Implementation of the DSOLoader
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "DSOLoader.hpp"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "constellation/build.hpp"
#include "constellation/core/logging/log.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/exec/exceptions.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::exec;
using namespace constellation::log;
using namespace constellation::satellite;
using namespace constellation::utils;

DSOLoader::DSOLoader(std::string dso_name, Logger& logger, std::filesystem::path hint) : dso_name_(std::move(dso_name)) {
    // Possible paths:
    // - custom executable: hint
    // - in dev environment: builddir/satellites/XYZ/libXYZ.suffix
    // - in installed environment: libdir/ConstellationSatellites/libXYZ.suffix
    const auto dso_file_name = "lib" + dso_name_ + CNSTLN_DSO_SUFFIX;
    auto possible_paths = std::vector<std::filesystem::path>({
        std::filesystem::path(CNSTLN_BUILDDIR) / "cxx" / "satellites" / dso_name_ / dso_file_name,
        std::filesystem::path(CNSTLN_LIBDIR) / "ConstellationSatellites" / dso_file_name,
    });
    if(!hint.empty()) {
        possible_paths.insert(possible_paths.begin(), std::move(hint));
    }

    // Check files following priority
    std::string path_str {};
    for(const auto& path : possible_paths) {
        const auto abs_path = std::filesystem::absolute(path);
        LOG(logger, TRACE) << "Looking for " << dso_name_ << " in " << abs_path;
        if(std::filesystem::exists(abs_path) && std::filesystem::is_regular_file(abs_path)) {
            LOG(logger, DEBUG) << "Found " << dso_name_ << " in " << abs_path;
            path_str = abs_path.string();
            break;
        }
    }
    if(path_str.empty()) {
        throw DSOLoadingError(dso_name_, "Could not find " + dso_file_name);
    }

    // Load the DSO
#ifdef _WIN32
    handle_ = static_cast<void*>(LoadLibrary(path_str.c_str()));
    if(handle_ == nullptr) {
        const auto last_win_error = std::system_category().message(GetLastError());
        throw DSOLoadingError(dso_name_, last_win_error);
    }
#else
    handle_ = dlopen(path_str.c_str(), RTLD_NOW);
    if(handle_ == nullptr) {
        const auto* error_dlopen = dlerror(); // NOLINT(concurrency-mt-unsafe)
        throw DSOLoadingError(dso_name_, error_dlopen);
    }
#endif

    LOG(logger, DEBUG) << "Loaded shared library " << dso_file_name;
}

DSOLoader::~DSOLoader() {
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(handle_));
#else
    dlclose(handle_);
#endif
}

Generator* DSOLoader::loadSatelliteGenerator() {
    return getFunctionFromDSO<satellite::Generator>("generator");
}

void* DSOLoader::get_raw_function_from_dso(const std::string& function_name) {
#ifdef _WIN32
    void* function = reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle_), function_name.c_str()));
    if(function == nullptr) {
        const auto last_win_error = std::system_category().message(GetLastError());
        throw DSOFunctionLoadingError(function_name, dso_name_, last_win_error);
    }
#else
    void* function = dlsym(handle_, function_name.c_str());
    if(function == nullptr) {
        const auto* error_dlsym = dlerror(); // NOLINT(concurrency-mt-unsafe)
        throw DSOFunctionLoadingError(function_name, dso_name_, error_dlsym);
    }
#endif

    return function;
}
