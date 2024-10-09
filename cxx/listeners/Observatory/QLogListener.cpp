/**
 * @file
 * @brief Log Listener Implementation
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QLogListener.hpp"

#include <iomanip>
#include <mutex>
#include <set>
#include <string>
#include <utility>

#include <magic_enum.hpp>

#include <QAbstractListModel>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVariant>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/pools/SubscriberPool.hpp"
#include "constellation/core/utils/string.hpp"

#include "listeners/Observatory/QLogMessage.hpp"

using namespace constellation::message;
using namespace constellation::chirp;
using namespace constellation::log;
using namespace constellation::pools;
using namespace constellation::utils;

QLogListener::QLogListener(QObject* parent)
    : QAbstractListModel(parent), SubscriberPool<CMDP1LogMessage, MONITORING>(
                                      "LOGRECV", [this](auto&& arg) { add_message(std::forward<decltype(arg)>(arg)); }),
      logger_("QLGRCV") {}

std::set<std::string> QLogListener::get_global_subscription_topics() const {
    std::set<std::string> topics;

    for(const auto level : magic_enum::enum_values<Level>()) {
        if(level >= subscription_global_level_) {
            topics.emplace("LOG/" + to_string(level));
        }
    }
    return topics;
}

void QLogListener::subscribeToTopic(constellation::log::Level level, std::string_view topic) {

    subscription_global_level_ = level;

    constexpr auto levels = magic_enum::enum_values<Level>();
    for(const auto lvl : levels) {
        std::string log_topic = "LOG/" + to_string(lvl);
        if(!topic.empty()) {
            log_topic += "/";
            log_topic += topic;
        }

        if(level <= lvl) {
            LOG(logger_, DEBUG) << "Subscribing to " << std::quoted(log_topic);
            subscribe(log_topic);
        } else {
            LOG(logger_, DEBUG) << "Unsubscribing from " << std::quoted(log_topic);
            unsubscribe(log_topic);
        }
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

    const std::lock_guard message_lock {message_mutex_};

    // We always add new messages to the end of the deque:
    const auto pos = static_cast<int>(messages_.size());
    beginInsertRows(QModelIndex(), pos, pos);
    messages_.emplace_back(std::move(msg));
    message_count_.store(messages_.size());
    endInsertRows();

    // send signal:
    emit newMessage(createIndex(pos, 0));
}

void QLogListener::host_connected(const DiscoveredService& service) {
    // Subscribe to all current global topics:
    for(const auto& topic : get_global_subscription_topics()) {
        subscribe(service.host_id, topic);
    }

    // Emit the signal with the current number of connections
    emit connectionsChanged(countSockets());
}

void QLogListener::host_disconnected(const DiscoveredService& /*service*/) {
    // Emit the signal with the current number of connections
    emit connectionsChanged(countSockets());
}

QVariant QLogListener::data(const QModelIndex& index, int role) const {
    if(role != Qt::DisplayRole || !index.isValid()) {
        return {};
    }

    const std::lock_guard message_lock {message_mutex_};
    if(index.column() < QLogMessage::countColumns() && index.row() < static_cast<int>(messages_.size())) {
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

void QLogListener::setGlobalSubscriptionLevel(Level level) {
    LOG(logger_, DEBUG) << "Updating global subscription level to " << to_string(level);
    subscription_global_level_ = level;
    subscribeToTopic(level);
}
