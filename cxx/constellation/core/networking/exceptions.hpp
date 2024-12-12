/**
 * @file
 * @brief Network communication exceptions used in the framework
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <string>
#include <utility>

#include "constellation/build.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/string.hpp"

namespace constellation::networking {

    /**
     * @ingroup Exceptions
     * @brief Errors related to network communication
     *
     * Problems that could never have been detected at compile time
     */
    class CNSTLN_API NetworkError : public utils::RuntimeError {
    public:
        /**
         * @brief Creates exception with the given network problem
         * @param what_arg Text describing the problem
         */
        explicit NetworkError(std::string what_arg) : RuntimeError(std::move(what_arg)) {}

    protected:
        /**
         * @brief Internal constructor for exceptions setting the error message indirectly
         */
        NetworkError() = default;
    };

    /**
     * @ingroup Exceptions
     * @brief Error when sending a message timed out
     */
    class CNSTLN_API SendTimeoutError : public NetworkError {
    public:
        explicit SendTimeoutError(const std::string& what, std::chrono::seconds timeout) {
            error_message_ = "Failed sending " + what + " after " + utils::to_string<std::chrono::seconds>(timeout);
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Error when receiving a message timed out
     */
    class CNSTLN_API RecvTimeoutError : public NetworkError {
    public:
        explicit RecvTimeoutError(const std::string& what, std::chrono::seconds timeout) {
            error_message_ = "Failed receiving " + what + " after " + utils::to_string<std::chrono::seconds>(timeout);
        }
    };

} // namespace constellation::networking
