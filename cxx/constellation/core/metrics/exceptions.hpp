/**
 * @file
 * @brief Metric exceptions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <string_view>

#include "constellation/build.hpp"
#include "constellation/core/utils/exceptions.hpp"

namespace constellation::metrics {
    /**
     * @ingroup Exceptions
     * @brief Value of a metric has an invalid type
     */
    class CNSTLN_API InvalidMetricValueException : public utils::LogicError {
    public:
        explicit InvalidMetricValueException(std::string_view name, std::string_view type) {
            error_message_ = "Metric \"";
            error_message_ += name;
            error_message_ = "\" with type ";
            error_message_ += type;
            error_message_ += " cannot be cast to metric value";
        }
    };
} // namespace constellation::metrics
