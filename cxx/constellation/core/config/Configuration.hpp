/**
 * @file
 * @brief Configuration class
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <atomic>
#include <filesystem>
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <zmq.hpp>

#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/config/exceptions.hpp"

namespace constellation::config {

    /**
     * @brief Generic configuration object storing keys
     *
     * The configuration holds a set of keys with arbitrary values that are internally stored as std::variant.
     */
    class CNSTLN_API Configuration {

        /**
         * @brief Helper class to keep track of key access
         *
         * This class holds all configuration keys in a map together with an atomic boolean marking whether they have been
         * accessed already. This allows to find out which keys have not been accessed at all. This wrapper allows to use
         * atomics for non-locking access but requires to register all keys beforehand.
         */
        class AccessMarker {
        public:
            /**
             * Default constructor
             */
            AccessMarker() = default;
            ~AccessMarker() = default;

            /**
             * @brief Explicit copy constructor to allow copying of the map keys
             */
            AccessMarker(const AccessMarker& other);

            /**
             * @brief Explicit copy assignment operator to allow copying of the map keys
             */
            AccessMarker& operator=(const AccessMarker& other);

            // Default move constructor/assignment
            AccessMarker(AccessMarker&& other) noexcept = default;
            AccessMarker& operator=(AccessMarker&& other) = default;

            /**
             * @brief Method to register a key for a new access marker
             * @param key Key of the marker
             * @warning This operation is not thread-safe
             */
            void registerMarker(const std::string& key);

            /**
             * @brief Method to mark existing marker as accessed/used.
             * @param key Key of the marker
             * @note This is an atomic operation and thread-safe.
             * @throws std::out_of_range if the key has not been registered beforehand
             */
            void markUsed(const std::string& key) { markers_.at(key).store(true); };

            /**
             * @brief Method to retrieve access status of an existing marker.
             * @param key Key of the marker
             * @note This is an atomic operation and thread-safe.
             * @throws std::out_of_range if the key has not been registered beforehand
             */
            bool isUsed(const std::string& key) { return markers_.at(key).load(); }

        private:
            std::map<std::string, std::atomic_bool> markers_;
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
         */
        Configuration(const Dictionary& dict);

        // Default copy/move constructor/assignment
        Configuration(const Configuration& other) = default;
        Configuration& operator=(const Configuration& other) = default;
        Configuration(Configuration&& other) noexcept = default;
        Configuration& operator=(Configuration&& other) = default;

        /**
         * @brief Check if key is defined
         * @param key Key to check for existence
         * @return True if key exists, false otherwise
         */
        bool has(const std::string& key) const { return config_.contains(key); }

        /**
         * @brief Check how many of the given keys are defined
         *
         * This is useful to check if two or more conflicting config keys that are defined.
         *
         * @param keys Keys to check for existence
         * @return number of existing keys from the given list
         */
        std::size_t count(std::initializer_list<std::string> keys) const;

        /**
         * @brief Get value of a key in requested type
         * @param key Key to get value of
         * @return Value of the key in the type of the requested template parameter
         *
         * @throws MissingKeyError If the requested key is not defined
         * @throws InvalidTypeError If the conversion to the requested type did not succeed
         * @throws InvalidTypeError If an overflow happened while converting the key
         */
        template <typename T> T get(const std::string& key) const;

        /**
         * @brief Get value of a key in requested type or default value if it does not exists
         * @param key Key to get value of
         * @param def Default value to use if key is not defined
         * @return Value of the key in the type of the requested template parameter
         *         or the default value if the key does not exists
         *
         * @throws InvalidKeyError If the conversion to the requested type did not succeed
         * @throws InvalidKeyError If an overflow happened while converting the key
         */
        template <typename T> T get(const std::string& key, const T& def) const;

        /**
         * @brief Get values for a key containing an array
         * @param key Key to get values of
         * @return List of values in the array in the requested template parameter
         *
         * @throws MissingKeyError If the requested key is not defined
         * @throws InvalidKeyError If the conversion to the requested type did not succeed
         * @throws InvalidKeyError If an overflow happened while converting the key
         */
        template <typename T> std::vector<T> getArray(const std::string& key) const { return get<std::vector<T>>(key); }

        /**
         * @brief Get values for a key containing an array or default array if it does not exists
         * @param key Key to get values of
         * @param def Default value array to use if key is not defined
         * @return List of values in the array in the requested template parameter
         *         or the default array if the key does not exist
         *
         * @throws InvalidKeyError If the conversion to the requested type did not succeed
         * @throws InvalidKeyError If an overflow happened while converting the key
         */
        template <typename T> std::vector<T> getArray(const std::string& key, const std::vector<T>& def) const;

        /**
         * @brief Get literal value of a key as string
         * @param key Key to get values of
         * @return Literal value of the key
         * @note This function does also not remove quotation marks in strings
         */
        std::string getText(const std::string& key) const;

        /**
         * @brief Get literal value of a key as string or a default if it does not exists
         * @param key Key to get values of
         * @param def Default value to use if key is not defined
         * @return Literal value of the key or the default value if the key does not exists
         * @note This function does also not remove quotation marks in strings
         */
        std::string getText(const std::string& key, const std::string& def) const;

        /**
         * @brief Get absolute path to file with paths relative to the configuration
         * @param key Key to get path of
         * @param check_exists If the file should be checked for existence (if yes always returns a canonical path)
         * @return Absolute path to a file
         *
         * @throws InvalidValueError If the path did not exists while the check_exists parameter is given
         */
        std::filesystem::path getPath(const std::string& key, bool check_exists = false) const;

        /**
         * @brief Get absolute path to file with paths relative to the configuration
         * @param key Key to get path of
         * @param extension File extension to be added to path if not present
         * @param check_exists If the file should be checked for existence (if yes always returns a canonical path)
         * @return Absolute path to a file
         *
         * @throws InvalidValueError If the path did not exists while the check_exists parameter is given
         */
        std::filesystem::path getPathWithExtension(const std::string& key,
                                                   const std::string& extension,
                                                   bool check_exists = false) const;

        /**
         * @brief Get array of absolute paths to files with paths relative to the configuration
         * @param key Key to get path of
         * @param check_exists If the files should be checked for existence (if yes always returns a canonical path)
         * @return List of absolute path to all the requested files
         *
         * @throws InvalidValueError If the path did not exists while the check_exists parameter is given
         */
        std::vector<std::filesystem::path> getPathArray(const std::string& key, bool check_exists = false) const;

        /**
         * @brief Set value for a key in a given type
         * @param key Key to set value of
         * @param val Value to assign to the key
         * @param mark_used Flag whether key should be marked as "used" directly
         */
        template <typename T> void set(const std::string& key, const T& val, bool mark_used = false);

        /**
         * @brief Set list of values for a key in a given type
         * @param key Key to set values of
         * @param val List of values to assign to the key
         * @param mark_used Flag whether key should be marked as "used" directly
         */
        template <typename T> void setArray(const std::string& key, const std::vector<T>& val, bool mark_used = false) {
            set<std::vector<T>>(key, val, mark_used);
        }

        /**
         * @brief Set default value for a key only if it is not defined yet
         * @param key Key to possible set value of
         * @param val Value to assign if the key is not defined yet
         * @note This marks the default key as "used" automatically
         */
        template <typename T> void setDefault(const std::string& key, const T& val);

        /**
         * @brief Set default list of values for a key only if it is not defined yet
         * @param key Key to possible set values of
         * @param val List of values to assign to the key if the key is not defined yet
         * @note This marks the default key as "used" automatically
         */
        template <typename T> void setDefaultArray(const std::string& key, const std::vector<T>& val);

        /**
         * @brief Set alias name for an already existing key
         * @param new_key New alias to be created
         * @param old_key Key the alias is created for
         * @param warn Optionally print a warning message to notify of deprecation
         * @note This marks the old key as "used" automatically
         */
        void setAlias(const std::string& new_key, const std::string& old_key, bool warn = false);

        /**
         * @brief Return total number of key / value pairs
         *
         * This also counts internal keys.
         *
         * @return Number of settings
         */
        std::size_t size() const { return config_.size(); }

        /**
         * @brief Update with keys from another configuration, potentially overriding keys in this configuration
         * @param other Configuration to merge this one with
         */
        void merge(const Configuration& other);

        /**
         * @brief Get all key value pairs
         * @return List of all key value pairs
         */
        // FIXME Better name for this function
        Dictionary getAll() const;

        /**
         * @brief Obtain all keys which have not been accessed yet
         *
         * This method returns all keys from the configuration object which have not yet been accessed, Default values as
         * well as aliases are marked as used automatically and are therefore never returned.
         */
        std::vector<std::string> getUnusedKeys() const;

        /**
         * @brief Assemble configuration via msgpack for ZeroMQ
         *
         * @note This does not embedded the used keys, just the tag / value pairs
         * @param used_only Select whether to only include keys that have been used
         */
        std::shared_ptr<zmq::message_t> assemble(bool used_only = false) const;

    private:
        /**
         * @brief Make relative paths absolute from this configuration file
         * @param path Path to make absolute (if it is not already absolute)
         * @param canonicalize_path If the path should be canonicalized (throws an error if the path does not exist)
         * @throws std::invalid_argument If the path does not exists
         */
        static std::filesystem::path path_to_absolute(std::filesystem::path path, bool canonicalize_path);

        /**
         * @brief Get all key value pairs which have been used
         * @return List of all used key value pairs, including internal ones
         */
        Dictionary get_used_entries() const;

        Dictionary config_;
        mutable AccessMarker used_keys_;
    };

} // namespace constellation::config

// Include template members
#include "Configuration.tpp"
