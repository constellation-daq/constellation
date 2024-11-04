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
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "constellation/build.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/exec/exceptions.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::exec;
using namespace constellation::log;
using namespace constellation::satellite;
using namespace constellation::utils;

DSOLoader::DSOLoader(const std::string& dso_name, Logger& logger, const std::filesystem::path& hint) {
    // Possible paths:
    // - custom executable: hint
    // - in dev environment: builddir/satellites/XYZ/libXYZ.suffix
    // - in installed environment: libdir/ConstellationSatellites/libXYZ.suffix
    const auto dso_file_name = to_dso_file_name(dso_name);
    LOG(logger, TRACE) << "Searching paths for library with name " << dso_file_name;

    // List containing paths of possible DSOs (ordered after priority)
    auto possible_paths = std::vector<std::filesystem::path>();

    auto add_file_path = [&](const std::filesystem::path& path) {
        const auto abs_path = std::filesystem::absolute(path);
        // Check that path is a file
        if(std::filesystem::is_regular_file(abs_path)) {
            // Check that file name matches (case-insensitive)
            if(transform(abs_path.filename().string(), ::tolower) == transform(dso_file_name, ::tolower)) {
                LOG(logger, TRACE) << "Adding " << abs_path << " to library lookup";
                possible_paths.emplace_back(abs_path);
            }
        }
    };

    auto add_path = [&](const std::filesystem::path& path) {
        const auto abs_path = std::filesystem::absolute(path);
        // For directories, recursively iterate and add paths
        if(std::filesystem::is_directory(abs_path)) {
            for(const auto& entry : std::filesystem::recursive_directory_iterator(abs_path)) {
                // Try adding path as file
                add_file_path(entry.path());
            }
        } else {
            // If not directory, try adding as file
            add_file_path(abs_path);
        }
    };

    // Hint has highest priority:
    if(!hint.empty()) {
        add_path(hint);
    }

    const auto build_dir = std::filesystem::path(CNSTLN_BUILDDIR) / "cxx" / "satellites";
    add_path(build_dir);

    const auto lib_dir = std::filesystem::path(CNSTLN_LIBDIR) / "ConstellationSatellites";
    add_path(lib_dir);

    // Did not find a matching library:
    if(possible_paths.empty()) {
        throw DSOLoadingError(dso_name, "Could not find " + dso_file_name);
    }

    // Get found path with highest priority
    const std::filesystem::path library_path = possible_paths.front();

    // Get actual DSO name from path
    dso_name_ = library_path.stem().string().substr(std::string(CNSTLN_DSO_PREFIX).size());

    // Load the DSO
#ifdef _WIN32
    handle_ = static_cast<void*>(LoadLibrary(library_path.string().c_str()));
    if(handle_ == nullptr) {
        const auto last_win_error = std::system_category().message(GetLastError());
        throw DSOLoadingError(dso_name_, last_win_error);
    }
#else
    handle_ = dlopen(library_path.c_str(), RTLD_NOW);
    if(handle_ == nullptr) {
        const auto* error_dlopen = dlerror(); // NOLINT(concurrency-mt-unsafe)
        throw DSOLoadingError(dso_name_, error_dlopen);
    }
#endif

    LOG(logger, DEBUG) << "Loaded shared library " << library_path.filename();
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
