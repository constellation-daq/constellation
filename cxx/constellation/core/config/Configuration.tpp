/**
 * @file
 * @brief Template implementation of configuration
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "Configuration.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <magic_enum.hpp>

#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/config/exceptions.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/string.hpp"

namespace constellation::config {

    template <typename T> T Configuration::get(const std::string& key) const {
        try {
            const auto dictval = config_.at(key);
            T val;
            // Value is directly held by variant:
            if constexpr(is_one_of<T, value_t>()) {
                val = std::get<T>(dictval);
            } else if constexpr(std::is_arithmetic_v<T>) {
                if(std::holds_alternative<std::int64_t>(dictval)) {
                    val = static_cast<T>(std::get<std::int64_t>(dictval));
                } else {
                    val = static_cast<T>(std::get<double>(dictval));
                }
            } else if constexpr(std::is_enum_v<T>) {
                const auto str = std::get<std::string>(dictval);
                const auto enum_val = magic_enum::enum_cast<T>(utils::transform(str, ::toupper));

                if(!enum_val.has_value()) {
                    throw std::invalid_argument("possible values are " + utils::list_enum_names<T>());
                }

                val = enum_val.value();
            } else {
                throw std::bad_variant_access();
            }
            used_keys_.markUsed(key);
            return val;
        } catch(std::out_of_range& e) {
            /* Requested key has not been found in dictionary */
            throw MissingKeyError(key);
        } catch(std::bad_variant_access& e) {
            /* Value held by the dictionary entry could not be cast to desired type */
            throw InvalidTypeError(key, config_.at(key).type(), typeid(T));
        } catch(std::invalid_argument& e) {
            /* Value held by the dictionary entry could not be converted to desired type */
            throw InvalidValueError(config_.at(key).str(), key, e.what());
        }
    }

    template <typename T> T Configuration::get(const std::string& key, const T& def) const {
        if(has(key)) {
            return get<T>(key);
        }
        return def;
    }

    template <typename T> std::vector<T> Configuration::getArray(const std::string& key) const {
        try {
            // Value is directly held by variant, let's return:
            if constexpr(is_one_of<std::vector<T>, value_t>()) {
                return get<std::vector<T>>(key);
            } else if constexpr(std::is_arithmetic_v<T>) {
                const auto dictval = config_.at(key);
                if(std::holds_alternative<std::vector<std::int64_t>>(dictval)) {
                    const auto vec = get<std::vector<std::int64_t>>(key);
                    return std::vector<T>(vec.begin(), vec.end());
                } else {
                    const auto vec = get<std::vector<double>>(key);
                    return std::vector<T>(vec.begin(), vec.end());
                }
            } else if constexpr(std::is_enum_v<T>) {
                const auto vec = get<std::vector<std::string>>(key);
                std::vector<T> result {};
                result.reserve(vec.size());

                std::for_each(vec.begin(), vec.end(), [&](const auto& str) {
                    const auto enum_val = magic_enum::enum_cast<T>(utils::transform(str, ::toupper));
                    if(!enum_val.has_value()) {
                        throw std::invalid_argument("possible values are " + utils::list_enum_names<T>());
                    }
                    result.emplace_back(enum_val.value());
                });
                return result;
            } else {
                return get<std::vector<T>>(key);
            }
        } catch(std::out_of_range& e) {
                /* Requested key has not been found in dictionary */
            throw MissingKeyError(key);
        } catch(std::invalid_argument& e) {
                /* Value held by the dictionary entry could not be converted to desired type */
            throw InvalidValueError(config_.at(key).str(), key, e.what());
        }
    }

    template <typename T> std::vector<T> Configuration::getArray(const std::string& key, const std::vector<T>& def) const {
        if(has(key)) {
            return getArray<T>(key);
        }
        return def;
    }

    template <typename T> void Configuration::set(const std::string& key, const T& val, bool mark_used) {
        if constexpr(is_one_of<T, value_t>()) {
            config_[key] = val;
        } else if constexpr(std::is_integral_v<T>) {
            if (val > std::numeric_limits<std::int64_t>::max()) {
                // TODO: throw
            }
            config_[key] = static_cast<std::int64_t>(val);
        } else if constexpr(std::is_floating_point_v<T>) {
            config_[key] = static_cast<double>(val);
        } else if constexpr(std::is_enum_v<T>) {
            config_[key] = utils::to_string(val);
        } else {
            // FIXME throw something?
            config_[key] = val;
        }
        used_keys_.registerMarker(key);
        if(mark_used) {
            used_keys_.markUsed(key);
        }
    }

    template <typename T> void Configuration::setArray(const std::string& key, const std::vector<T>& val, bool mark_used) {
        if constexpr(is_one_of<std::vector<T>, value_t>()) {
            set<std::vector<T>>(key, val, mark_used);
        } else if constexpr(std::is_integral_v<T>) {
            std::vector<std::int64_t> nval {};
            nval.reserve(val.size());
            for (auto val_elem : val) {
                if (val_elem > std::numeric_limits<std::int64_t>::max()) {
                    // TODO: throw
                }
                nval.emplace_back(static_cast<std::int64_t>(val_elem));
            }
            set(key, nval, mark_used);
        } else if constexpr(std::is_floating_point_v<T>) {
            const std::vector<double> nval {val.begin(), val.end()};
            set(key, nval, mark_used);
        } else if constexpr(std::is_enum_v<T>) {
            std::vector<std::string> nval {};
            nval.reserve(val.size());

            std::for_each(val.begin(), val.end(), [&](const auto& enum_val) {
                nval.emplace_back(utils::to_string(enum_val));
            });
            set(key, nval, mark_used);
        } else {
            // FIXME throw something?
            set<std::vector<T>>(key, val, mark_used);
        }
    }

    template <typename T> void Configuration::setDefault(const std::string& key, const T& val) {
        if(!has(key)) {
            set<T>(key, val, true);
        }
    }

    template <typename T> void Configuration::setDefaultArray(const std::string& key, const std::vector<T>& val) {
        if(!has(key)) {
            setArray<T>(key, val, true);
        }
    }
} // namespace constellation::config
