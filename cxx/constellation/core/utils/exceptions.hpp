/**
 * @file
 * @brief Base exceptions used in the framework
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

/**
 * @defgroup Exceptions Exception classes
 * @brief Collection of all the exceptions used in the framework
 */

#pragma once

#include <exception>
#include <string>
#include <utility>

#include "constellation/build.hpp"

namespace constellation::utils {

    /**
     * @ingroup Exceptions
     * @brief Base class for all non-internal exceptions in framework.
     */
    class CNSTLN_API Exception : public std::exception {
    public:
        /**
         * @brief Creates exception with the specified problem
         * @param what_arg Text describing the problem
         */
        explicit Exception(std::string what_arg) : error_message_(std::move(what_arg)) {}

        /**
         * @brief Return the error message
         * @return Text describing the error
         */
        const char* what() const noexcept override { return error_message_.c_str(); }

    protected:
        /**
         * @brief Internal constructor for exceptions setting the error message indirectly
         */
        Exception() = default;

        // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
        std::string error_message_;
    };

    /**
     * @ingroup Exceptions
     * @brief Errors related to problems occurring at runtime
     *
     * Problems that could never have been detected at compile time
     */
    class CNSTLN_API RuntimeError : public Exception {
    public:
        /**
         * @brief Creates exception with the given runtime problem
         * @param what_arg Text describing the problem
         */
        explicit RuntimeError(std::string what_arg) : Exception(std::move(what_arg)) {}

    protected:
        /**
         * @brief Internal constructor for exceptions setting the error message indirectly
         */
        RuntimeError() = default;
    };

    /**
     * @ingroup Exceptions
     * @brief Errors related to logical problems in the code structure
     *
     * Problems that could also have been detected at compile time by specialized software
     */
    class CNSTLN_API LogicError : public Exception {
    public:
        /**
         * @brief Creates exception with the given logical problem
         * @param what_arg Text describing the problem
         */
        explicit LogicError(std::string what_arg) : Exception(std::move(what_arg)) {}

    protected:
        /**
         * @brief Internal constructor for exceptions setting the error message indirectly
         */
        LogicError() = default;
    };

} // namespace constellation::utils
