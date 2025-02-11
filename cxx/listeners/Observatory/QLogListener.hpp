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
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>

#include <QAbstractListModel>
#include <QObject>
#include <QRegularExpression>
#include <QString>
#include <QVariant>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/gui/QLogMessage.hpp"
#include "constellation/listener/LogListener.hpp"

class QLogListener : public QAbstractListModel, public constellation::listener::LogListener {
    Q_OBJECT

public:
    /**
     * @brief Constructor of QLogListener
     *
     * @param parent QObject parent
     */
    explicit QLogListener(QObject* parent = nullptr);

    /**
     * @brief Obtain a message from a given QModelIndex
     *
     * @param index QModelIndex of the message
     * @return Constant reference to the message
     */
    const constellation::gui::QLogMessage& getMessage(const QModelIndex& index) const;

    /**
     * @brief Clear all currently stored messages
     */
    void clearMessages();

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
    int columnCount(const QModelIndex& /*parent*/) const override { return constellation::gui::QLogMessage::countColumns(); }
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
     * @param level The log level of the newly added message
     */
    void newMessage(QModelIndex index, constellation::log::Level level);

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

    void newTopics(QStringList topics);

private:
    /**
     * @brief Callback registered for receiving log messages from the subscription pool
     * This function inserts the message into storage, checks for new topics or sender names to register, and emits a
     * newMessage signal with the inserted index when the message is displayed.
     *
     * @param msg Received log message
     */
    void add_message(constellation::message::CMDP1LogMessage&& msg);

    void host_connected(const constellation::chirp::DiscoveredService& service) override;
    void host_disconnected(const constellation::chirp::DiscoveredService& service) override;

    void topics_available(std::string_view sender, const std::map<std::string, std::string>& topics) override;

private:
    /** Log messages & access mutex*/
    std::deque<constellation::gui::QLogMessage> messages_;
    std::atomic_size_t message_count_;
    mutable std::mutex message_mutex_;

    /** Available senders and topics */
    std::set<std::string> sender_list_;
    std::set<std::string> topic_list_;
};
