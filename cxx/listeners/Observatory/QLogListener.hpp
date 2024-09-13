/**
 * @file
 * @brief Log Listsner as QAbstractList
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <QAbstractListModel>
#include <QRegularExpression>
#include <vector>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/pools/SubscriberPool.hpp"

class LogMessage : public constellation::message::CMDP1LogMessage {
public:
    explicit LogMessage(constellation::message::CMDP1LogMessage msg);

    QString operator[](int) const;
    std::string Text(int) const;

    static int NumColumns() { return headers_.size(); }

    static QString ColumnName(int i);
    static int ColumnWidth(int);

    // Column headers of the log details
    static constexpr std::array<const char*, 5> headers_ {"Time", "Sender", "Level", "Topic", "Message"};
};

class LogSorter {
public:
    LogSorter(std::vector<LogMessage>* messages);
    void SetSort(int col, bool ascending);
    bool operator()(size_t lhs, size_t rhs);

private:
    std::vector<LogMessage>* m_msgs;
    int m_col;
    bool m_asc;
};

class QLogListener : public QAbstractListModel,
                     public constellation::pools::SubscriberPool<constellation::message::CMDP1LogMessage,
                                                                 constellation::chirp::MONITORING> {
    Q_OBJECT

public:
    QLogListener(QObject* parent = 0);

    constellation::log::Level GetLevel(const QModelIndex& index) const;
    bool IsDisplayed(size_t index);

    void setFilterLevel(constellation::log::Level level);
    constellation::log::Level getFilterLevel() const { return filter_level_; }

    /**
     * @brief Set a new sender filter value
     *
     * @param sender Sender filter
     * @return True if the filter was updated, false otherwise
     */
    bool setFilterSender(const std::string& sender);
    std::string getFilterSender() const { return filter_sender_; }

    /**
     * @brief Set a new topic filter value
     *
     * @param topic Topic filter
     * @return True if the filter was updated, false otherwise
     */
    bool setFilterTopic(const std::string& topic);
    std::string getFilterTopic() const { return filter_topic_; }

    /**
     * @brief Set a new message filter value
     *
     * @param pattern Message filter pattern
     */
    void setFilterMessage(const QString& topic);
    QString getFilterMessage() const { return filter_message_.pattern(); }

    void setGlobalSubscriptionLevel(constellation::log::Level level);
    constellation::log::Level getGlobalSubscriptionLevel() const { return subscription_global_level_; }

    void UpdateDisplayed();

    const LogMessage& GetMessage(int row) const;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    void sort(int column, Qt::SortOrder order) override;

    void subscribeToTopic(constellation::log::Level level, std::string_view topic = "");

signals:
    /**
     * @brief Signal emitted whenever a new message has been added
     * @param index QModelIndex at which the message has been inserted
     */
    void newMessage(QModelIndex index);

    /**
     * @brief Signal emitted whenever a message from a new sender has been received
     * @param sender Canonical name of the sender
     */
    void newSender(QString sender);

    /**
     * @brief Signal emitted whenever a message with a new topic has been received
     * @param topic Topic
     */
    void newTopic(QString topic);

private:
    void add_message(constellation::message::CMDP1LogMessage&& msg);

private:
    /** Logger to use */
    constellation::log::Logger logger_;

    /** Subscription & storage */
    std::vector<LogMessage> m_all;
    constellation::log::Level subscription_global_level_ {constellation::log::Level::INFO};

    /** Filter & display */
    std::vector<size_t> m_disp;
    constellation::log::Level filter_level_ {constellation::log::Level::TRACE};
    std::set<std::string> filter_sender_list_ {"- All -"};
    std::string filter_sender_ {"- All -"};
    std::set<std::string> filter_topic_list_ {"- All -"};
    std::string filter_topic_ {"- All -"};

    QRegularExpression filter_message_;

    LogSorter m_sorter;
};
