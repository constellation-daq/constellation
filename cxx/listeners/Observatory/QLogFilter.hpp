/**
 * @file
 * @brief Log Filter
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <set>
#include <string>
#include <vector>

#include "constellation/core/log/Logger.hpp"

#include "QLogListener.hpp"

class QLogFilter : public QSortFilterProxyModel {

    Q_OBJECT

public:
    QLogFilter(QObject* parent = nullptr);

    /**
     * @brief Method to filter rows
     * @details In this method, all currently set filters are allied to the given row of the source model. It checks for
     * log level, sender, topic match and compares the message to the filter regular expression.
     *
     * @param sourceRow Row of the respective message in the source model
     * @param sourceParent QModelIndex of the parent item
     *
     * @return True if the message passes the filters, false otherwise
     */
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

    /**
     * @brief Set a new log level filter value
     *
     * @param level New log level filter
     */
    void setFilterLevel(constellation::log::Level level);

    /**
     * @brief Return the currently set log level filter
     * @return Log level filter
     */
    constellation::log::Level getFilterLevel() const { return filter_level_; }

    /**
     * @brief Set a new sender filter value
     *
     * @param sender Sender filter
     * @return True if the filter was updated, false otherwise
     */
    void setFilterSender(const std::string& sender);

    /**
     * @brief Return the currently set sender filter
     * @return Sender filter
     */
    std::string getFilterSender() const { return filter_sender_; }

    /**
     * @brief Set a new topic filter value
     *
     * @param topic Topic filter
     * @return True if the filter was updated, false otherwise
     */
    void setFilterTopic(const std::string& topic);

    /**
     * @brief Return the currently set topic filter
     * @return Topic filter
     */
    std::string getFilterTopic() const { return filter_topic_; }

    /**
     * @brief Set a new message filter value
     *
     * @param pattern Message filter pattern
     */
    void setFilterMessage(const QString& pattern);

    /**
     * @brief Return the currently set message filter pattern
     * @return Message filter pattern
     */
    QString getFilterMessage() const { return filter_message_.pattern(); }

private:
    /**
     * @brief Helper to determine if a message at the given index should be displayed currently.
     *
     * This evaluates the currently-configured filters.
     *
     * @param index Index of the message in the message storage
     * @return True if the message is to be displayed, false otherwise
     */
    bool is_message_displayed(size_t index) const;

private:
    /** Logger to use */
    constellation::log::Logger logger_;

    /** Filters */
    constellation::log::Level filter_level_ {constellation::log::Level::TRACE};
    std::string filter_sender_ {"- All -"};
    std::string filter_topic_ {"- All -"};
    QRegularExpression filter_message_;
};
