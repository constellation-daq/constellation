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
#include <utility>

#include <QApplication>
#include <QColor>
#include <QDateTime>
#include <QPalette>
#include <QString>
#include <QTimeZone>

#include "constellation/core/log/Level.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/std_future.hpp"

#if __cpp_lib_format >= 201907L
#include <format>
#endif

namespace constellation::gui {

    /**
     * @brief Helper to obtain the state string with color and formatting
     *
     * @param state State to obtain string for
     * @param global Marker if the state is global or not
     *
     * @return String for the state display
     */
    inline QString get_styled_state(protocol::CSCP::State state, bool global) {

        const QString global_indicatior = (global ? "" : " ≊");

        switch(state) {
        case protocol::CSCP::State::NEW: {
            return "<font color='gray'><b>New</b>" + global_indicatior + "</font>";
        }
        case protocol::CSCP::State::initializing: {
            return "<font color='gray'><b>Initializing...</b>" + global_indicatior + "</font>";
        }
        case protocol::CSCP::State::INIT: {
            return "<font color='gray'><b>Initialized</b>" + global_indicatior + "</font>";
        }
        case protocol::CSCP::State::launching: {
            return "<font color='orange'><b>Launching...</b>" + global_indicatior + "</font>";
        }
        case protocol::CSCP::State::landing: {
            return "<font color='orange'><b>Landing...</b>" + global_indicatior + "</font>";
        }
        case protocol::CSCP::State::reconfiguring: {
            return "<font color='orange'><b>Reconfiguring...</b>" + global_indicatior + "</font>";
        }
        case protocol::CSCP::State::ORBIT: {
            return "<font color='orange'><b>Orbiting</b>" + global_indicatior + "</font>";
        }
        case protocol::CSCP::State::starting: {
            return "<font color='green'><b>Starting...</b>" + global_indicatior + "</font>";
        }
        case protocol::CSCP::State::stopping: {
            return "<font color='green'><b>Stopping...</b>" + global_indicatior + "</font>";
        }
        case protocol::CSCP::State::RUN: {
            return "<font color='green'><b>Running</b>" + global_indicatior + "</font>";
        }
        case protocol::CSCP::State::SAFE: {
            return "<font color='red'><b>Safe Mode</b>" + global_indicatior + "</font>";
        }
        case protocol::CSCP::State::interrupting: {
            return "<font color='red'><b>Interrupting...</b>" + global_indicatior + "</font>";
        }
        case protocol::CSCP::State::ERROR: {
            return "<font color='darkred'><b>Error</b>" + global_indicatior + "</font>";
        }
        default: std::unreachable();
        }
    }

    /**
     * @brief Helper to obtain the CSCP message type string with color and formatting
     *
     * @param type CSCP message type
     *
     * @return String for the CSCP response display
     */
    inline QString get_styled_response(message::CSCP1Message::Type type) {
        const auto type_string = QString::fromStdString(utils::enum_name(type));
        switch(type) {
        case message::CSCP1Message::Type::REQUEST:
        case message::CSCP1Message::Type::NOTIMPLEMENTED: {
            return "<font color='gray'>" + type_string + "</font>";
        }
        case message::CSCP1Message::Type::SUCCESS: {
            return "<font color='green'>" + type_string + "</font>";
        }
        case message::CSCP1Message::Type::INCOMPLETE:
        case message::CSCP1Message::Type::INVALID:
        case message::CSCP1Message::Type::UNKNOWN: {
            return "<font color='orange'>" + type_string + "</font>";
        }
        case message::CSCP1Message::Type::ERROR: {
            return "<font color='darkred'>" + type_string + "</font>";
        }
        default: std::unreachable();
        }
    }

    /** Color assignment for log levels */
    inline QColor get_log_level_color(log::Level level) {
        switch(level) {
        case log::Level::TRACE:
        case log::Level::DEBUG: {
            return QColorConstants::Gray;
        }
        case log::Level::INFO: {
            // Neutral:
            return QApplication::palette().text().color();
        }
        case log::Level::WARNING: {
            return {255, 138, 0, 128};
        }
        case log::Level::STATUS: {
            return {0, 100, 0, 128};
        }
        case log::Level::CRITICAL: {
            return {255, 0, 0, 128};
        }
        case log::Level::OFF: {
            return {0, 0, 0, 128};
        }
        default: std::unreachable();
        }
    };

    inline QDateTime from_timepoint(const std::chrono::system_clock::time_point& time_point) {
#if __cpp_lib_chrono >= 201907L && QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
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
} // namespace constellation::gui
