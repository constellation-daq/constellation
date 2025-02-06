/**
 * @file
 * @brief Log Listener Implementation
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QLogListener.hpp"

#include <cstddef>
#include <mutex>
#include <set>
#include <string>
#include <utility>

#include <QAbstractListModel>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVariant>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/gui/QLogMessage.hpp"
#include "constellation/listener/LogListener.hpp"

using namespace constellation::message;
using namespace constellation::chirp;
using namespace constellation::gui;
using namespace constellation::log;
using namespace constellation::pools;
using namespace constellation::utils;

QLogListener::QLogListener(QObject* parent)
    : QAbstractListModel(parent),
      constellation::listener::LogListener("LOG", [this](auto&& arg) { add_message(std::forward<decltype(arg)>(arg)); }) {}

void QLogListener::clearMessages() {
    message_count_.store(0);
    const std::lock_guard message_lock {message_mutex_};
    if(!messages_.empty()) {
        beginRemoveRows(QModelIndex(), 0, static_cast<int>(messages_.size() - 1));
        messages_.clear();
        endRemoveRows();
    }
}

void QLogListener::add_message(CMDP1LogMessage&& msg) {

    const auto [s_it, s_inserted] = sender_list_.emplace(msg.getHeader().getSender());
    if(s_inserted) {
        emit newSender(QString::fromStdString(std::string(msg.getHeader().getSender())));
    }

    const auto [t_it, t_inserted] = topic_list_.emplace(msg.getLogTopic());
    if(t_inserted) {
        emit newTopic(QString::fromStdString(std::string(msg.getLogTopic())));
    }

    const auto level = msg.getLogLevel();

    // Add new message to the end of the deque
    std::size_t messages_size {};
    {
        const std::lock_guard message_lock {message_mutex_};
        messages_.emplace_back(std::move(msg));
        messages_size = messages_.size();
    }
    message_count_.store(messages_size);

    // Call insert rows afterwards for less time with mutex
    const auto pos = static_cast<int>(messages_size - 1);
    beginInsertRows(QModelIndex(), pos, pos);
    endInsertRows();

    // Send signal
    emit newMessage(createIndex(pos, 0), level);
}

void QLogListener::host_connected(const DiscoveredService& service) {
    LogListener::host_connected(service);

    // Emit the signal with the current number of connections
    emit connectionsChanged(countSockets());
}

void QLogListener::host_disconnected(const DiscoveredService& service) {
    LogListener::host_disconnected(service);

    // Emit the signal with the current number of connections
    emit connectionsChanged(countSockets());
}

QVariant QLogListener::data(const QModelIndex& index, int role) const {
    if(role != Qt::DisplayRole || !index.isValid()) {
        return {};
    }

    if(index.column() < QLogMessage::countColumns() && index.row() < static_cast<int>(message_count_.load())) {
        const std::lock_guard message_lock {message_mutex_};
        return messages_[index.row()][index.column()];
    }

    return {};
}

const QLogMessage& QLogListener::getMessage(const QModelIndex& index) const {
    const std::lock_guard message_lock {message_mutex_};
    return messages_[index.row()];
}

QVariant QLogListener::headerData(int column, Qt::Orientation orientation, int role) const {

    if(role == Qt::DisplayRole && orientation == Qt::Horizontal && column >= 0 && column < QLogMessage::countColumns()) {
        return QLogMessage::columnName(column);
    }

    return {};
}
