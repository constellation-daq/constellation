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

int LogMessage::columnWidth(int i) {
    switch(i) {
    case 0: return 150;
    case 1: return 120;
    case 2: return 90;
    case 3: return 90;
    default: return -1;
    }
}

QString LogMessage::operator[](int column) const {
    switch(column) {
    case 0:
        return QString::fromStdString(
            std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::time_point_cast<std::chrono::seconds>(getHeader().getTime())));
    case 1: return QString::fromStdString(std::string(getHeader().getSender()));
    case 2: return QString::fromStdString(to_string(getLogLevel()));
    case 3: return QString::fromStdString(std::string(getLogTopic()));
    case 4: return QString::fromStdString(std::string(getLogMessage()));
    case 5: return QString::fromStdString(getHeader().getTags().to_string());
    default: return "";
    }
}

QString LogMessage::columnName(int i) {
    if(i < 0 || i >= static_cast<int>(headers_.size())) {
        return {};
    }
    return headers_[i];
}

LogSorter::LogSorter(std::deque<LogMessage>* messages) : m_msgs(messages), m_col(0), m_asc(true) {}

void LogSorter::SetSort(int col, bool ascending) {
    m_col = col;
    m_asc = ascending;
}

bool LogSorter::operator()(size_t lhs, size_t rhs) {
    const auto l = (*m_msgs)[lhs][m_col];
    const auto r = (*m_msgs)[rhs][m_col];
    return m_asc ^ (QString::compare(l, r, Qt::CaseInsensitive) < 0);
}

QLogListener::QLogListener(QObject* parent)
    : QAbstractListModel(parent), SubscriberPool<CMDP1LogMessage, MONITORING>(
                                      "LOGRECV", [this](auto&& arg) { add_message(std::forward<decltype(arg)>(arg)); }),
      logger_("QLGRCV"), filter_level_(Level::WARNING), m_sorter(&messages_) {
    // Make filtering case-insensitive
    filter_message_.setPatternOptions(QRegularExpression::CaseInsensitiveOption);

    // Set default subscription topics:
    setSubscriptionTopics(get_global_subscription_topics());
}

bool QLogListener::is_message_displayed(size_t index) const {
    const LogMessage& msg = messages_[index];
    const auto match = filter_message_.match(QString::fromStdString(std::string(msg.getLogMessage())));
    return (msg.getLogLevel() >= filter_level_) &&
           (msg.getHeader().getSender() == filter_sender_ || "- All -" == filter_sender_) &&
           (msg.getLogTopic() == filter_topic_ || "- All -" == filter_topic_) && match.hasMatch();
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

    const auto [s_it, s_inserted] = filter_sender_list_.emplace(msg.getHeader().getSender());
    if(s_inserted) {
        emit newSender(QString::fromStdString(std::string(msg.getHeader().getSender())));
    }

    const auto [t_it, t_inserted] = filter_topic_list_.emplace(msg.getLogTopic());
    if(t_inserted) {
        emit newTopic(QString::fromStdString(std::string(msg.getLogTopic())));
    }

    messages_.emplace_back(std::move(msg));
    if(is_message_displayed(messages_.size() - 1)) {
        std::vector<size_t>::iterator it =
            std::lower_bound(display_indices_.begin(), display_indices_.end(), messages_.size() - 1, m_sorter);
        size_t pos = it - display_indices_.begin();
        beginInsertRows(QModelIndex(), pos, pos);
        display_indices_.insert(it, messages_.size() - 1);
        endInsertRows();

        // send signal:
        emit newMessage(createIndex(pos, 0));
    }
    // return QModelIndex(); // FIXME we need to tell someone that we changed it...? Probably not because display not changed
}

void QLogListener::update_displayed_messages() {
    if(!display_indices_.empty()) {
        beginRemoveRows(createIndex(0, 0), 0, display_indices_.size() - 1);
        display_indices_.clear();
        endRemoveRows();
    }
    std::vector<size_t> disp;
    for(size_t i = 0; i < messages_.size(); ++i) {
        if(is_message_displayed(i)) {
            disp.push_back(i);
        }
    }
    std::sort(disp.begin(), disp.end(), m_sorter);
    if(disp.size() > 0) {
        beginInsertRows(createIndex(0, 0), 0, disp.size() - 1);
        display_indices_ = disp;
        endInsertRows();
    }
}

int QLogListener::rowCount(const QModelIndex& /*parent*/) const {
    return display_indices_.size();
}

int QLogListener::columnCount(const QModelIndex& /*parent*/) const {
    return LogMessage::countColumns();
}

Level QLogListener::getMessageLevel(const QModelIndex& index) const {
    return messages_[display_indices_[index.row()]].getLogLevel();
}

QVariant QLogListener::data(const QModelIndex& index, int role) const {
    if(role != Qt::DisplayRole || !index.isValid()) {
        return QVariant();
    }

    if(index.column() < columnCount() && index.row() < rowCount()) {
        return getMessage(index)[index.column()];
    }

    return QVariant();
}

const LogMessage& QLogListener::getMessage(const QModelIndex& index) const {
    return messages_[display_indices_[index.row()]];
}

QVariant QLogListener::headerData(int section, Qt::Orientation orientation, int role) const {
    if(role != Qt::DisplayRole) {
        return QVariant();
    }

    if(orientation == Qt::Horizontal && section < columnCount()) {
        return LogMessage::columnName(section);
    }

    return QVariant();
}

void QLogListener::sort(int column, Qt::SortOrder order) {
    m_sorter.SetSort(column, order == Qt::AscendingOrder);
    update_displayed_messages();
}

void QLogListener::setFilterLevel(Level level) {
    LOG(logger_, DEBUG) << "Updating filter level to " << to_string(level);
    filter_level_ = level;
    update_displayed_messages();
}

void QLogListener::setGlobalSubscriptionLevel(Level level) {
    LOG(logger_, DEBUG) << "Updating global subscription level to " << to_string(level);
    subscription_global_level_ = level;
    subscribeToTopic(level);
}

bool QLogListener::setFilterSender(const std::string& sender) {
    if(filter_sender_list_.contains(sender)) {
        LOG(logger_, DEBUG) << "Updating filter sender to " << sender;
        filter_sender_ = sender;
        update_displayed_messages();
        return true;
    }
    return false;
}

bool QLogListener::setFilterTopic(const std::string& topic) {
    if(filter_topic_list_.contains(topic)) {
        LOG(logger_, DEBUG) << "Updating filter topic to " << topic;
        filter_topic_ = topic;
        update_displayed_messages();
        return true;
    }
    return false;
}

void QLogListener::setFilterMessage(const QString& pattern) {
    LOG(logger_, DEBUG) << "Updating filter pattern for message to " << pattern.toStdString();
    filter_message_.setPattern(pattern);
    update_displayed_messages();
}
