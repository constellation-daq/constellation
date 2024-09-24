/**
 * @file
 * @brief Qt utilities
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <QDateTime>
#include <QTimeZone>

namespace constellation::controller {

    QDateTime from_timepoint(const std::chrono::system_clock::time_point& time_point) {
#if __cpp_lib_chrono >= 201907L
        return QDateTime::fromStdTimePoint(std::chrono::time_point_cast<std::chrono::milliseconds>(time_point));
#else
        return QDateTime(QDate(1970, 1, 1), QTime(0, 0, 0), QTimeZone::utc())
            .addMSecs(std::chrono::duration_cast<std::chrono::milliseconds>(time_point.time_since_epoch()).count());
#endif
    }
} // namespace constellation::controller
