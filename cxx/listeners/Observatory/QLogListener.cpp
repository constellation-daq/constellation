/**
 * @file
 * @brief Log Listener Implementation
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QLogListener.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <iostream>
#include <QDateTime>
#include <QVariant>
#include <set>
#include <string>
#include <vector>

#include <magic_enum.hpp>

#include "constellation/core/log/Level.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::message;
using namespace constellation::chirp;
using namespace constellation::log;
using namespace constellation::pools;
using namespace constellation::utils;

LogMessage::LogMessage(CMDP1LogMessage&& msg) : CMDP1LogMessage(std::move(msg)) {}

int LogMessage::columnWidth(int column) {
    switch(column) {
    case 0: return 150;
    case 1: return 120;
    case 2: return 90;
    case 3: return 95;
    default: return -1;
    }
}

QVariant LogMessage::operator[](int column) const {
    switch(column) {
    case 0:
        return QDateTime::fromStdTimePoint(std::chrono::time_point_cast<std::chrono::milliseconds>(getHeader().getTime()));
    case 1: return QString::fromStdString(std::string(getHeader().getSender()));
    case 2: return QString::fromStdString(to_string(getLogLevel()));
    case 3: return QString::fromStdString(std::string(getLogTopic()));
    case 4: return QString::fromStdString(std::string(getLogMessage()));
    case 5: return QString::fromStdString(getHeader().getTags().to_string());
    default: return "";
    }
}

QString LogMessage::columnName(int column) {
    if(column < 0 || column >= static_cast<int>(headers_.size())) {
        return {};
    }
    return headers_.at(column);
}

QLogListener::QLogListener(QObject* parent)
    : QAbstractListModel(parent), SubscriberPool<CMDP1LogMessage, MONITORING>(
                                      "LOGRECV", [this](auto&& arg) { add_message(std::forward<decltype(arg)>(arg)); }),
      logger_("QLGRCV") {
    // Set default subscription topics:
    setSubscriptionTopics(get_global_subscription_topics());
}

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

    // Update default subscription topics:
    setSubscriptionTopics(get_global_subscription_topics());

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

    // We always add new messages to the end of the deque:
    const auto pos = static_cast<int>(messages_.size());
    beginInsertRows(QModelIndex(), pos, pos);
    messages_.emplace_back(std::move(msg));
    endInsertRows();

    // send signal:
    emit newMessage(createIndex(pos, 0));
}

int QLogListener::rowCount(const QModelIndex& /*parent*/) const {
    return messages_.size();
}

int QLogListener::columnCount(const QModelIndex& /*parent*/) const {
    return LogMessage::countColumns();
}

Level QLogListener::getMessageLevel(const QModelIndex& index) const {
    return messages_[index.row()].getLogLevel();
}

QVariant QLogListener::data(const QModelIndex& index, int role) const {
    if(role != Qt::DisplayRole || !index.isValid()) {
        return {};
    }

    if(index.column() < LogMessage::countColumns() && index.row() < static_cast<int>(messages_.size())) {
        return getMessage(index)[index.column()];
    }

    return {};
}

const LogMessage& QLogListener::getMessage(const QModelIndex& index) const {
    return messages_[index.row()];
}

QVariant QLogListener::headerData(int column, Qt::Orientation orientation, int role) const {

    if(role == Qt::DisplayRole && orientation == Qt::Horizontal && column >= 0 && column < LogMessage::countColumns()) {
        return LogMessage::columnName(column);
    }

    return {};
}

void QLogListener::setGlobalSubscriptionLevel(Level level) {
    LOG(logger_, DEBUG) << "Updating global subscription level to " << to_string(level);
    subscription_global_level_ = level;
    subscribeToTopic(level);
}
