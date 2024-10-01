/**
 * @file
 * @brief Log Listener as QAbstractList
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <deque>
#include <QAbstractListModel>
#include <QRegularExpression>
#include <vector>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/pools/SubscriberPool.hpp"

/**
 * @class LogMessage
 * @brief Wrapper class around CMDP1 Log messages which provide additional accessors to make them play nice with the
 * QAbstractListModel they are used in.
 */
class LogMessage : public constellation::message::CMDP1LogMessage {
public:
    /**
     * @brief Constructing a Log message from a CMDP1LogMessage
     *
     * @param msg CMDP1 Log Message
     */
    explicit LogMessage(constellation::message::CMDP1LogMessage&& msg);

    /**
     * @brief Operator to fetch column information as string representation from the message
     *
     * @param column Column to retrieve the string representation for
     * @return String representation of the message info
     */
    QString operator[](int column) const;

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

class LogSorter {
public:
    LogSorter(std::deque<LogMessage>* messages);
    void SetSort(int col, bool ascending);
    bool operator()(size_t lhs, size_t rhs);

private:
    std::deque<LogMessage>* m_msgs;
    int m_col;
    bool m_asc;
};

class QLogListener : public QAbstractListModel,
                     public constellation::pools::SubscriberPool<constellation::message::CMDP1LogMessage,
                                                                 constellation::chirp::MONITORING> {
    Q_OBJECT

public:
    /**
     * @brief Constructor of QLogListener
     *
     * @param parent QObject parent
     */
    explicit QLogListener(QObject* parent = 0);

    /**
     * @brief Obtain the log level of a message at a given model index
     *
     * @param index QModelIndex of the message in question
     * @return Log level of the message
     */
    constellation::log::Level getMessageLevel(const QModelIndex& index) const;

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
    bool setFilterSender(const std::string& sender);

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
    bool setFilterTopic(const std::string& topic);

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

    /**
     * @brief Update the global subscription log level
     * @param level New global subscription log level
     */
    void setGlobalSubscriptionLevel(constellation::log::Level level);

    /**
     * @brief Get current global log subscription level
     * @return Global log subscription level
     */
    constellation::log::Level getGlobalSubscriptionLevel() const { return subscription_global_level_; }

    /**
     * @brief Obtain a message from a given QModelIndex
     *
     * @param index QModelIndex of the message
     * @return Constant reference to the message
     */
    const LogMessage& getMessage(const QModelIndex& index) const;

    /**
     * @brief Subscribe to a given topic at a given log level
     * @details This method subscribes automatically to all log levels higher than the chosen level. If no topic is provided,
     * the method subscribes to the global logging at the given level, i.e. all topics available.
     *
     * @note Currently only works on all attached senders
     *
     * @param level Desired subscription log level
     * @param topic Topic to subscribe to
     */
    void subscribeToTopic(constellation::log::Level level, std::string_view topic = "");

    /// @cond doxygen_suppress

    /* Qt accessor methods */
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    void sort(int column, Qt::SortOrder order) override;

    /// @endcond

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
    /**
     * @brief Callback registered for receiving log messages from the subscription pool
     * This function inserts the message into storage, checks for new topics or sender names to register, and emits a
     * newMessage signal with the inserted index when the message is displayed.
     *
     * \param msg Received log message
     */
    void add_message(constellation::message::CMDP1LogMessage&& msg);

    /**
     * @brief Helper to determine if a message at the given index should be displayed currently.
     *
     * This evaluates the currently-configured filters.
     *
     * @param index Index of the message in the message storage
     * @return True if the message is to be displayed, false otherwise
     */
    bool is_message_displayed(size_t index) const;

    /**
     * @brief Helper to update the list of displayed messages
     */
    void update_displayed_messages();

    /**
     * @brief Helper to get all subscription topics given a global subscription log level.This is used to immediately
     * subscribe new sockets appearing to the global log level and all topics below

     * @return Set of subscription topics
     */
    std::set<std::string> get_global_subscription_topics() const;

private:
    /** Logger to use */
    constellation::log::Logger logger_;

    /** Subscription & storage */
    std::deque<LogMessage> messages_;
    constellation::log::Level subscription_global_level_ {constellation::log::Level::INFO};

    /** Filter & display */
    std::vector<size_t> display_indices_;
    constellation::log::Level filter_level_ {constellation::log::Level::TRACE};
    std::set<std::string> filter_sender_list_ {"- All -"};
    std::string filter_sender_ {"- All -"};
    std::set<std::string> filter_topic_list_ {"- All -"};
    std::string filter_topic_ {"- All -"};
    QRegularExpression filter_message_;

    /** Sorting helper */
    LogSorter m_sorter;
};
