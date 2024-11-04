/**
 * @file
 * @brief Template implementation of configuration
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "Configuration.hpp" // NOLINT(misc-header-include-cycle)

#include <stdexcept>
#include <string>
#include <typeinfo>
#include <variant>
#include <vector>

#include "constellation/core/config/exceptions.hpp"
#include "constellation/core/config/Value.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/core/utils/type.hpp"

namespace constellation::config {

    template <typename T> T Configuration::get(const std::string& key) const {
        const auto key_lc = utils::transform(key, ::tolower);
        try {
            const auto& dictval = config_.at(key_lc);
            const auto val = dictval.get<T>();
            dictval.markUsed();
            return val;
        } catch(const std::out_of_range&) {
            // Requested key has not been found in dictionary
            throw MissingKeyError(key);
        } catch(const std::bad_variant_access&) {
            // Value held by the dictionary entry could not be cast to desired type
            throw InvalidTypeError(key, config_.at(key_lc).demangle(), utils::demangle<T>());
        } catch(const std::invalid_argument& error) {
            // Value held by the dictionary entry could not be converted to desired type
            throw InvalidValueError(getText(key), key, error.what());
        }
    }

    template <typename T> T Configuration::get(const std::string& key, const T& def) {
        setDefault<T>(key, def);
        return get<T>(key);
    }

    template <typename T> std::vector<T> Configuration::getArray(const std::string& key, const std::vector<T>& def) {
        return get<std::vector<T>>(key, def);
    }

    template <typename T> void Configuration::set(const std::string& key, const T& val, bool mark_used) {
        const auto key_lc = utils::transform(key, ::tolower);
        try {
            config_[key_lc] = {Value::set(val), mark_used};
        } catch(const std::bad_cast&) {
            // Value held by the dictionary entry could not be cast to desired type
            throw InvalidTypeError(key, utils::demangle<T>(), utils::demangle<value_t>());
        } catch(const std::overflow_error& error) {
            if constexpr(utils::convertible_to_string<T>) {
                throw InvalidValueError(utils::to_string(val), key, error.what());
            } else if constexpr(utils::convertible_range_to_string<T>) {
                throw InvalidValueError(utils::range_to_string(val), key, error.what());
            } else {
                throw InvalidValueError("<unknown>", key, error.what());
            }
        }
    }

    template <typename T> void Configuration::setDefault(const std::string& key, const T& val) {
        if(!has(key)) {
            set<T>(key, val, false);
        }
    }

    template <typename T> void Configuration::setDefaultArray(const std::string& key, const std::vector<T>& val) {
        if(!has(key)) {
            setArray<T>(key, val, false);
        }
    }

    template <typename F> void Configuration::for_each(Group group, Usage usage, F f) const {
        using enum Group;
        using enum Usage;
        for(const auto& [key, value] : config_) {
            if((group == ALL || (group == USER && !key.starts_with("_")) || (group == INTERNAL && key.starts_with("_"))) &&
               (usage == ANY || (usage == USED && value.isUsed()) || (usage == UNUSED && !value.isUsed()))) {
                f(key, value);
            }
        }
    }

} // namespace constellation::config
