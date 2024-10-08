/**
 * @file
 * @brief Log Listener as QAbstractList
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstddef>
#include <deque>
#include <mutex>
#include <QAbstractListModel>
#include <QObject>
#include <QRegularExpression>
#include <QString>
#include <QVariant>
#include <set>
#include <string>
#include <string_view>

#include <zmq.hpp>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/pools/SubscriberPool.hpp"

#include "listeners/Observatory/QLogMessage.hpp"

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
    explicit QLogListener(QObject* parent = nullptr);

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
    const QLogMessage& getMessage(const QModelIndex& index) const;

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

    /**
     * @brief Helper to check if a given sender is known already
     * This is used to e.g. cross-check filter settings
     *
     * @note the comparison here is not case-insensitive.
     *
     * @param sender Sender to be checked for
     * @return True if this sender has been sending messages, false otherwise
     */
    bool isSenderKnown(const std::string& sender) const { return sender_list_.contains(sender); }

    /**
     * @brief Helper to check if a given topic is known already
     * This is used to e.g. cross-check filter settings
     *
     * @note the comparison here is not case-insensitive.
     *
     * @param topic Topic to be checked for
     * @return True if this topic has been found in any message, false otherwise
     */
    bool isTopicKnown(const std::string& topic) const { return topic_list_.contains(topic); }

    /// @cond doxygen_suppress

    /* Qt accessor methods */
    int rowCount(const QModelIndex& /*parent*/) const override { return static_cast<int>(message_count_.load()); }
    int columnCount(const QModelIndex& /*parent*/) const override { return QLogMessage::countColumns(); }
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int column, Qt::Orientation orientation, int role) const override;

    /// @endcond

signals:
    /**
     * @brief Signal emitted whenever a connection changed
     * @param connections Number of currently held connections
     */
    void connectionsChanged(std::size_t connections);

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
     * @param msg Received log message
     */
    void add_message(constellation::message::CMDP1LogMessage&& msg);

    void socket_connected(zmq::socket_t& socket) override;
    void socket_disconnected(zmq::socket_t& socket) override;

    /**
     * @brief Helper to get all subscription topics given a global subscription log level.This is used to immediately
     * subscribe new sockets appearing to the global log level and all topics below

     * @return Set of subscription topics
     */
    std::set<std::string> get_global_subscription_topics() const;

private:
    /** Logger to use */
    constellation::log::Logger logger_;

    /** Log messages & access mutex*/
    std::deque<QLogMessage> messages_;
    std::atomic_size_t message_count_;
    mutable std::mutex message_mutex_;

    /** Subscriptions */
    constellation::log::Level subscription_global_level_ {constellation::log::Level::INFO};
    std::set<std::string> sender_list_;
    std::set<std::string> topic_list_;
};
