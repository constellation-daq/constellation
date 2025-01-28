/**
 * @file
 * @brief Log Message Implementation
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QLogMessage.hpp"

#include <string>
#include <utility>

#include <QDateTime>
#include <QString>
#include <QVariant>

#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/gui/qt_utils.hpp"

using namespace constellation::gui;
using namespace constellation::message;
using namespace constellation::utils;

QLogMessage::QLogMessage(CMDP1LogMessage&& msg) : CMDP1LogMessage(std::move(msg)) {}

int QLogMessage::columnWidth(int column) {
    switch(column) {
    case 0: return 150;
    case 1: return 120;
    case 2: return 90;
    case 3: return 95;
    default: return -1;
    }
}

QVariant QLogMessage::operator[](int column) const {
    switch(column) {
    case 0: return from_timepoint(getHeader().getTime());
    case 1: return QString::fromStdString(std::string(getHeader().getSender()));
    case 2: return QString::fromStdString(to_string(getLogLevel()));
    case 3: return QString::fromStdString(std::string(getLogTopic()));
    case 4: {
        // Trim string to first line break
        const auto msg = std::string(getLogMessage());
        const auto pos = msg.find_first_of('\n');
        if(pos != std::string::npos) {
            return QString::fromStdString(msg.substr(0, pos) + " [...]");
        }
        return QString::fromStdString(msg);
    }
    case 5: return QString::fromStdString(getHeader().getTags().to_string(false));
    case 6: return QString::fromStdString(std::string(getLogMessage()));
    default: return "";
    }
}

QString QLogMessage::columnName(int column) {
    if(column < 0 || column >= static_cast<int>(headers_.size())) {
        return {};
    }
    return headers_.at(column);
}
