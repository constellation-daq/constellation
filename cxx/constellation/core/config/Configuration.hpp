/**
 * @file
 * @brief Configuration class
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "constellation/build.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/config/exceptions.hpp"
#include "constellation/core/config/Value.hpp"
#include "constellation/core/utils/string.hpp"

namespace constellation::config {

    /**
     * @brief Generic configuration object storing keys
     *
     * The configuration holds a set of keys with arbitrary values that are internally stored as std::variant.
     */
    class Configuration {
    private:
        /**
         * @brief Helper class to keep track of key-value pair access
         */
        class ConfigValue : public Value {
        public:
            using Value::Value; // NOLINT(misc-include-cleaner)
            using Value::operator=;

            /**
             * @brief Construct ConfigValue from Value with predefined usage
             */
            ConfigValue(Value value, bool used = false) : Value(std::move(value)), used_(used) {}

            /**
             * @brief Method to mark ConfigValue as used/unused
             * @param used If the key-value pair should be marked used or unused
             */
            void markUsed(bool used = true) const { used_ = used; }

            /**
             * @brief Method to retrieve of ConfigValue is used/unused
             * @return true if used, false if unused
             */
            bool isUsed() const { return used_; }

        private:
            mutable bool used_ {false};
        };

    public:
        /**
         * @brief Construct an empty configuration object
         */
        Configuration() = default;
        ~Configuration() = default;

        /**
         * @brief Construct a configuration object from a dictionary
         *
         * @param dict Dictionary to construct config object from
         * @param mark_used Whether to mark the key-value pairs in the dict as used
         */
        CNSTLN_API Configuration(const Dictionary& dict, bool mark_used = false);

        // No copy constructor/assignment, default move constructor/assignment
        /// @cond doxygen_suppress
        Configuration(const Configuration& other) = delete;
        Configuration& operator=(const Configuration& other) = delete;
        Configuration(Configuration&& other) noexcept = default;
        Configuration& operator=(Configuration&& other) = default;
        /// @endcond

        enum class Group : std::uint8_t {
            /** All configuration key-value pairs, both user and internal */
            ALL,
            /** Configuration key-value pairs intended for framework users */
            USER,
            /** Configuration key-value paris intended for internal framework usage */
            INTERNAL,
        };

        enum class Usage : std::uint8_t {
            /** Both used and unused key-value pairs */
            ANY,
            /** Only used key-value pairs */
            USED,
            /** Only unused key-value paris */
            UNUSED,
        };

        /**
         * @brief Check if key is defined
         * @param key Key to check for existence
         * @return True if key exists, false otherwise
         *
         * @note Keys are handled case-insensitively
         */
        bool has(const std::string& key) const { return config_.contains(utils::transform(key, ::tolower)); }

        /**
         * @brief Check how many of the given keys are defined
         *
         * This is useful to check if two or more conflicting config keys that are defined.
         *
         * @param keys Keys to check for existence
         * @return number of existing keys from the given list
         *
         * @note Keys are handled case-insensitively
         */
        CNSTLN_API std::size_t count(std::initializer_list<std::string> keys) const;

        /**
         * @brief Get value of a key in requested type
         * @param key Key to get value of
         * @return Value of the key in the type of the requested template parameter
         *
         * @note Keys are handled case-insensitively
         *
         * @throws MissingKeyError If the requested key is not defined
         * @throws InvalidTypeError If the conversion to the requested type did not succeed
         * @throws InvalidTypeError If an overflow happened while converting the key
         */
        template <typename T> T get(const std::string& key) const;

        /**
         * @brief Get value of a key in requested type or default value if it does not exists
         * @param key Key to get value of
         * @param def Default value to set if key is not defined
         * @return Value of the key in the type of the requested template parameter
         *         or the default value if the key does not exists
         *
         * @note Keys are handled case-insensitively
         *
         * @throws InvalidKeyError If the conversion to the requested type did not succeed
         * @throws InvalidKeyError If an overflow happened while converting the key
         */
        template <typename T> T get(const std::string& key, const T& def);

        /**
         * @brief Get values for a key containing an array
         * @param key Key to get values of
         * @return List of values in the array in the requested template parameter
         *
         * @note Keys are handled case-insensitively
         *
         * @throws MissingKeyError If the requested key is not defined
         * @throws InvalidKeyError If the conversion to the requested type did not succeed
         * @throws InvalidKeyError If an overflow happened while converting the key
         */
        template <typename T> std::vector<T> getArray(const std::string& key) const { return get<std::vector<T>>(key); }

        /**
         * @brief Get values for a key containing an array or default array if it does not exists
         * @param key Key to get values of
         * @param def Default value array to set if key is not defined
         * @return List of values in the array in the requested template parameter
         *         or the default array if the key does not exist
         *
         * @note Keys are handled case-insensitively
         *
         * @throws InvalidKeyError If the conversion to the requested type did not succeed
         * @throws InvalidKeyError If an overflow happened while converting the key
         */
        template <typename T> std::vector<T> getArray(const std::string& key, const std::vector<T>& def);

        /**
         * @brief Get literal value of a key as string
         * @param key Key to get values of
         * @return Literal value of the key
         * @note This function does also not remove quotation marks in strings, Keys are handled case-insensitively
         */
        CNSTLN_API std::string getText(const std::string& key) const;

        /**
         * @brief Get absolute path to file with paths relative to the configuration
         * @param key Key to get path of
         * @param check_exists If the file should be checked for existence (if yes always returns a canonical path)
         * @return Absolute path to a file
         *
         * @note Keys are handled case-insensitively
         *
         * @throws InvalidValueError If the path did not exists while the check_exists parameter is given
         */
        CNSTLN_API std::filesystem::path getPath(const std::string& key, bool check_exists = false) const;

        /**
         * @brief Get absolute path to file with paths relative to the configuration
         * @param key Key to get path of
         * @param extension File extension to be added to path if not present
         * @param check_exists If the file should be checked for existence (if yes always returns a canonical path)
         * @return Absolute path to a file
         *
         * @note Keys are handled case-insensitively
         *
         * @throws InvalidValueError If the path did not exists while the check_exists parameter is given
         */
        CNSTLN_API std::filesystem::path getPathWithExtension(const std::string& key,
                                                              const std::string& extension,
                                                              bool check_exists = false) const;

        /**
         * @brief Get array of absolute paths to files with paths relative to the configuration
         * @param key Key to get path of
         * @param check_exists If the files should be checked for existence (if yes always returns a canonical path)
         * @return List of absolute path to all the requested files
         *
         * @note Keys are handled case-insensitively
         *
         * @throws InvalidValueError If the path did not exists while the check_exists parameter is given
         */
        CNSTLN_API std::vector<std::filesystem::path> getPathArray(const std::string& key, bool check_exists = false) const;

        /**
         * @brief Set value for a key in a given type
         * @param key Key to set value of
         * @param val Value to assign to the key
         * @param mark_used Flag whether key should be marked as "used" directly
         *
         * @note Keys are handled case-insensitively and stored in lower case.
         */
        template <typename T> void set(const std::string& key, const T& val, bool mark_used = false);

        /**
         * @brief Set list of values for a key in a given type
         * @param key Key to set values of
         * @param val List of values to assign to the key
         * @param mark_used Flag whether key should be marked as "used" directly
         *
         * @note Keys are handled case-insensitively and stored in lower case.
         */
        template <typename T> void setArray(const std::string& key, const std::vector<T>& val, bool mark_used = false) {
            set<std::vector<T>>(key, val, mark_used);
        }

        /**
         * @brief Set default value for a key only if it is not defined yet
         * @param key Key to possible set value of
         * @param val Value to assign if the key is not defined yet
         * @note This marks the default key as "used" automatically
         *
         * @note Keys are handled case-insensitively and stored in lower case.
         */
        template <typename T> void setDefault(const std::string& key, const T& val);

        /**
         * @brief Set default list of values for a key only if it is not defined yet
         * @param key Key to possible set values of
         * @param val List of values to assign to the key if the key is not defined yet
         * @note This marks the default key as "used" automatically
         *
         * @note Keys are handled case-insensitively and stored in lower case.
         */
        template <typename T> void setDefaultArray(const std::string& key, const std::vector<T>& val);

        /**
         * @brief Set alias name for an already existing key
         * @param new_key New alias to be created
         * @param old_key Key the alias is created for
         * @param warn Optionally print a warning message to notify of deprecation
         * @note This marks the old key as "used" automatically. Keys are handled case-insensitively and stored in lower
         * case.
         */
        CNSTLN_API void setAlias(const std::string& new_key, const std::string& old_key, bool warn = false);

        /**
         * @brief Get number of key-value pairs for specific group and usage setting
         *
         * @param group Enum to restrict group of key-value pairs to include
         * @param usage Enum to restrict usage status of key-value pairs to include
         *
         * @return Number of key-value pairs
         */
        CNSTLN_API std::size_t size(Group group = Group::ALL, Usage usage = Usage::ANY) const;

        /**
         * @brief Get dictionary with key-value pairs for specific group and usage setting
         *
         * @param group Enum to restrict group of key-value pairs to include
         * @param usage Enum to restrict usage status of key-value pairs to include
         *
         * @return Dictionary containing the key-value pairs
         */
        CNSTLN_API Dictionary getDictionary(Group group = Group::ALL, Usage usage = Usage::ANY) const;

        /**
         * @brief Update with keys from another configuration, potentially overriding keys in this configuration
         *
         * @note This function only updates values that are actually used
         *
         * @param other Configuration with updated values
         */
        CNSTLN_API void update(const Configuration& other);

    private:
        /**
         * @brief Make relative paths absolute from this configuration file
         * @param path Path to make absolute (if it is not already absolute)
         * @param canonicalize_path If the path should be canonicalized (throws an error if the path does not exist)
         * @throws std::invalid_argument If the path does not exists
         */
        static std::filesystem::path path_to_absolute(std::filesystem::path path, bool canonicalize_path);

        /**
         * @brief Calls a function for every key-value pair matching group and usage criteria
         *
         * @param group Enum to restrict group of key-value pairs to include
         * @param usage Enum to restrict usage of key-value pairs to include
         * @param f Function taking a string ref and a Value ref
         *
         */
        template <typename F> void for_each(Group group, Usage usage, F f) const;

        std::map<std::string, ConfigValue> config_;
    };

} // namespace constellation::config

// Include template members
#include "Configuration.ipp" // IWYU pragma: keep
