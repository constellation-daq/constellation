/**
 * @file
 * @brief Implementation of configuration
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Configuration.hpp"

#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <vector>

#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/config/exceptions.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::config;

Configuration::Configuration(const Dictionary& dict, bool mark_used) {
    for(const auto& [key, val] : dict) {
        config_.emplace(utils::transform(key, ::tolower), ConfigValue(val, mark_used));
    }
};

std::size_t Configuration::count(std::initializer_list<std::string> keys) const {
    if(keys.size() == 0) {
        throw std::invalid_argument("list of keys cannot be empty");
    }

    std::size_t found = 0;
    for(const auto& key : keys) {
        if(has(key)) {
            found++;
        }
    }
    return found;
}

std::string Configuration::getText(const std::string& key) const {
    try {
        const auto& dictval = config_.at(utils::transform(key, ::tolower));
        dictval.markUsed();
        return dictval.str();
    } catch(std::out_of_range& e) {
        throw MissingKeyError(key);
    }
}

/**
 * For a relative path the absolute path of the configuration file is prepended. Absolute paths are not changed.
 */
std::filesystem::path Configuration::getPath(const std::string& key, bool check_exists) const {
    try {
        return path_to_absolute(get<std::string>(key), check_exists);
    } catch(std::invalid_argument& e) {
        throw InvalidValueError(getText(key), key, e.what());
    }
}
/**
 * For a relative path the absolute path of the configuration file is prepended. Absolute paths are not changed.
 */
std::filesystem::path Configuration::getPathWithExtension(const std::string& key,
                                                          const std::string& extension,
                                                          bool check_exists) const {
    try {
        return path_to_absolute(std::filesystem::path(get<std::string>(key)).replace_extension(extension), check_exists);
    } catch(std::invalid_argument& e) {
        throw InvalidValueError(getText(key), key, e.what());
    }
}
/**
 * For all relative paths the absolute path of the configuration file is prepended. Absolute paths are not changed.
 */
std::vector<std::filesystem::path> Configuration::getPathArray(const std::string& key, bool check_exists) const {
    const auto vals = getArray<std::string>(key);
    std::vector<std::filesystem::path> path_array {};
    path_array.reserve(vals.size());

    // Convert all paths to absolute
    try {
        for(const auto& path : vals) {
            path_array.emplace_back(path_to_absolute(path, check_exists));
        }
    } catch(std::invalid_argument& e) {
        throw InvalidValueError(getText(key), key, e.what());
    }
    return path_array;
}

/**
 *  The alias is only used if new key does not exist but old key does. The old key is automatically marked as used.
 */
void Configuration::setAlias(const std::string& new_key, const std::string& old_key, bool warn) {
    if(!has(old_key) || has(new_key)) {
        return;
    }

    const auto new_key_lc = utils::transform(new_key, ::tolower);
    const auto old_key_lc = utils::transform(old_key, ::tolower);
    config_[new_key_lc] = config_.at(old_key_lc);

    LOG_IF(WARNING, warn) << "Parameter " << std::quoted(old_key) << " is deprecated and superseded by "
                          << std::quoted(new_key);
}

std::filesystem::path Configuration::path_to_absolute(std::filesystem::path path, bool canonicalize_path) {
    // If not a absolute path, make it an absolute path
    if(!path.is_absolute()) {
        // Get current directory and append the relative path
        path = std::filesystem::current_path() / path;
    }

    // Normalize path only if we have to check if it exists
    // NOTE: This throws an error if the path does not exist
    if(canonicalize_path) {
        try {
            path = std::filesystem::canonical(path);
        } catch(std::filesystem::filesystem_error&) {
            throw std::invalid_argument("path " + path.string() + " not found");
        }
    }
    return path;
}

std::size_t Configuration::size(Group group, Usage usage) const {
    std::size_t size = 0;
    for_each(group, usage, [&](const std::string&, const Value&) { ++size; });
    return size;
}

Dictionary Configuration::getDictionary(Group group, Usage usage) const {
    Dictionary result {};
    for_each(group, usage, [&](const std::string& key, const Value& val) { result.emplace(key, val); });
    return result;
}

void Configuration::update(const Configuration& other) {
    // We only update the used keys from the other configuration
    for(const auto& [key, value] : other.getDictionary(Group::ALL, Usage::USED)) {
        set(key, value, true);
    }
}
