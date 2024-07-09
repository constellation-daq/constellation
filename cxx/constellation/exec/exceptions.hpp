/**
 * @file
 * @brief Collection of all exec library errors
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <string>
#include <string_view>

#include "constellation/build.hpp"
#include "constellation/core/utils/exceptions.hpp"

namespace constellation::exec {
    /**
     * @ingroup Exceptions
     * @brief Error while interacting with a Dynamic Shared Object (DSO)
     */
    class CNSTLN_API DSOLoaderError : public utils::RuntimeError {
    protected:
        DSOLoaderError() = default;
    };

    /**
     * @ingroup Exceptions
     * @brief Error while loading a Dynamic Shared Object (DSO)
     */
    class CNSTLN_API DSOLoadingError : public DSOLoaderError {
    public:
        explicit DSOLoadingError(std::string_view dso_name, std::string_view reason) {
            error_message_ = "Error while loading shared library \"";
            error_message_ += dso_name;
            error_message_ += "\": ";
            error_message_ += reason;
        }
    };

    /**
     * @ingroup Exceptions
     * @brief Requested function not found in Dynamic Shared Object (DSO)
     */
    class CNSTLN_API DSOFunctionLoadingError : public DSOLoaderError {
    public:
        explicit DSOFunctionLoadingError(std::string_view function, std::string_view dso_name, std::string_view reason) {
            error_message_ = "Error while loading function \"";
            error_message_ += function;
            error_message_ += "\" from shared library \"";
            error_message_ += dso_name;
            error_message_ += "\": ";
            error_message_ += reason;
        }
    };
} // namespace constellation::exec
