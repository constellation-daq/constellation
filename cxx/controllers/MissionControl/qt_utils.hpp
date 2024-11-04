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
#include <QString>
#include <QTimeZone>

#if __cpp_lib_format >= 201907L
#include <format>
#endif

namespace constellation::controller {

    inline QDateTime from_timepoint(const std::chrono::system_clock::time_point& time_point) {
#if __cpp_lib_chrono >= 201907L
        return QDateTime::fromStdTimePoint(std::chrono::time_point_cast<std::chrono::milliseconds>(time_point));
#else
        return QDateTime(QDate(1970, 1, 1), QTime(0, 0, 0), QTimeZone::utc())
            .addMSecs(std::chrono::duration_cast<std::chrono::milliseconds>(time_point.time_since_epoch()).count());
#endif
    }

    inline QString duration_string(std::chrono::seconds duration) {
#if __cpp_lib_format >= 201907L
        return QString::fromStdString(std::format("{:%H:%M:%S}", duration));
#else
        const std::chrono::hh_mm_ss<std::chrono::seconds> fdur {duration};
        return QString("%1:%2:%3")
            .arg(fdur.hours().count(), 2, 10, QChar('0'))
            .arg(fdur.minutes().count(), 2, 10, QChar('0'))
            .arg(fdur.seconds().count(), 2, 10, QChar('0'));
#endif
    }
} // namespace constellation::controller
