/**
 * @file
 * @brief Log Listener as QAbstractList
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <array>
#include <QRegularExpression>
#include <QVariant>

#include "constellation/core/message/CMDP1Message.hpp"

/**
 * @class LogMessage
 * @brief Wrapper class around CMDP1 Log messages which provide additional accessors to make them play nice with the
 * QAbstractListModel they are used in.
 */
class QLogMessage : public constellation::message::CMDP1LogMessage {
public:
    /**
     * @brief Constructing a Log message from a CMDP1LogMessage
     *
     * @param msg CMDP1 Log Message
     */
    explicit QLogMessage(constellation::message::CMDP1LogMessage&& msg);

    /**
     * @brief Operator to fetch column information as string representation from the message
     *
     * @param column Column to retrieve the string representation for
     * @return Variant with the respective message information
     */
    QVariant operator[](int column) const;

    /**
     * @brief Obtain number of info columns this message provides
     * @return Number of columns
     */
    static int countColumns() { return headers_.size() - 1; }

    /**
     * @brief Obtain number of info columns this message provides including extra information
     * @return Number of all columns
     */
    static int countExtendedColumns() { return headers_.size(); }

    /**
     * @brief Get title of a given column
     * @param column Column number
     * @return Header of the column
     */
    static QString columnName(int column);

    /**
     * @brief Obtain predefined width of a column
     * @param column Column number
     * @return Width of the column
     */
    static int columnWidth(int column);

private:
    // Column headers of the log details
    static constexpr std::array<const char*, 6> headers_ {"Time", "Sender", "Level", "Topic", "Message", "Tags"};
};
