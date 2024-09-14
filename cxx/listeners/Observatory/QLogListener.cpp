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

LogMessage::LogMessage(CMDP1LogMessage msg) : CMDP1LogMessage(std::move(msg)) {}

int LogMessage::ColumnWidth(int i) {
    switch(i) {
    case 0: return 150;
    case 1: return 120;
    case 2: return 90;
    case 3: return 90;
    default: return -1;
    }
}

QString LogMessage::operator[](int i) const {
    return Text(i).c_str();
}

std::string LogMessage::Text(int i) const {
    switch(i) {
    case 0:
        return std::format("{:%Y-%m-%d %H:%M:%S}",
                           std::chrono::time_point_cast<std::chrono::seconds>(getHeader().getTime()));
    case 1: return std::string(getHeader().getSender());
    case 2: return to_string(getLogLevel());
    case 3: return std::string(getLogTopic());
    case 4: return std::string(getLogMessage());
    case 5: return getHeader().getTags().to_string();
    default: return "";
    }
}

QString LogMessage::ColumnName(int i) {
    if(i < 0 || i >= static_cast<int>(headers_.size())) {
        return {};
    }
    return headers_[i];
}

LogSorter::LogSorter(std::vector<LogMessage>* messages) : m_msgs(messages), m_col(0), m_asc(true) {}

void LogSorter::SetSort(int col, bool ascending) {
    m_col = col;
    m_asc = ascending;
}

bool LogSorter::operator()(size_t lhs, size_t rhs) {
    QString l = (*m_msgs)[lhs].Text(m_col).c_str();
    QString r = (*m_msgs)[rhs].Text(m_col).c_str();
    return m_asc ^ (QString::compare(l, r, Qt::CaseInsensitive) < 0);
}

QLogListener::QLogListener(QObject* parent)
    : QAbstractListModel(parent), SubscriberPool<CMDP1LogMessage, MONITORING>(
                                      "LOGRECV",
                                      [this](auto&& arg) { add_message(std::forward<decltype(arg)>(arg)); },
                                      [this]() { return get_global_subscription_topics(); }),
      logger_("QLGRCV"), filter_level_(Level::WARNING), m_sorter(&m_all) {
    filter_message_.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
}

bool QLogListener::IsDisplayed(size_t index) {
    LogMessage& msg = m_all[index];
    const auto match = filter_message_.match(QString::fromStdString(std::string(msg.getLogMessage())));
    return (msg.getLogLevel() >= filter_level_) &&
           (msg.getHeader().getSender() == filter_sender_ || "- All -" == filter_sender_) &&
           (msg.getLogTopic() == filter_topic_ || "- All -" == filter_topic_) && match.hasMatch();
}

std::set<std::string> QLogListener::get_global_subscription_topics() {
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

    const auto [s_it, s_inserted] = filter_sender_list_.emplace(msg.getHeader().getSender());
    if(s_inserted) {
        emit newSender(QString::fromStdString(std::string(msg.getHeader().getSender())));
    }

    const auto [t_it, t_inserted] = filter_topic_list_.emplace(msg.getLogTopic());
    if(t_inserted) {
        emit newTopic(QString::fromStdString(std::string(msg.getLogTopic())));
    }

    m_all.emplace_back(std::move(msg));
    if(IsDisplayed(m_all.size() - 1)) {
        std::vector<size_t>::iterator it = std::lower_bound(m_disp.begin(), m_disp.end(), m_all.size() - 1, m_sorter);
        size_t pos = it - m_disp.begin();
        beginInsertRows(QModelIndex(), pos, pos);
        m_disp.insert(it, m_all.size() - 1);
        endInsertRows();

        // send signal:
        emit newMessage(createIndex(pos, 0));
    }
    // return QModelIndex(); // FIXME we need to tell someone that we changed it...? Probably not because display not changed
}

void QLogListener::UpdateDisplayed() {
    if(m_disp.size() > 0) {
        beginRemoveRows(createIndex(0, 0), 0, m_disp.size() - 1);
        m_disp.clear();
        endRemoveRows();
    }
    std::vector<size_t> disp;
    for(size_t i = 0; i < m_all.size(); ++i) {
        if(IsDisplayed(i)) {
            disp.push_back(i);
        }
    }
    std::sort(disp.begin(), disp.end(), m_sorter);
    if(disp.size() > 0) {
        beginInsertRows(createIndex(0, 0), 0, disp.size() - 1);
        m_disp = disp;
        endInsertRows();
    }
}

int QLogListener::rowCount(const QModelIndex& /*parent*/) const {
    return m_disp.size();
}

int QLogListener::columnCount(const QModelIndex& /*parent*/) const {
    return LogMessage::NumColumns();
}

Level QLogListener::GetLevel(const QModelIndex& index) const {
    return m_all[m_disp[index.row()]].getLogLevel();
}

QVariant QLogListener::data(const QModelIndex& index, int role) const {
    if(role != Qt::DisplayRole || !index.isValid()) {
        return QVariant();
    }

    if(index.column() < columnCount() && index.row() < rowCount()) {
        return GetMessage(index.row())[index.column()];
    }

    return QVariant();
}

const LogMessage& QLogListener::GetMessage(int row) const {
    return m_all[m_disp[row]];
}

QVariant QLogListener::headerData(int section, Qt::Orientation orientation, int role) const {
    if(role != Qt::DisplayRole) {
        return QVariant();
    }

    if(orientation == Qt::Horizontal && section < columnCount()) {
        return LogMessage::ColumnName(section);
    }

    return QVariant();
}

void QLogListener::sort(int column, Qt::SortOrder order) {
    m_sorter.SetSort(column, order == Qt::AscendingOrder);
    UpdateDisplayed();
}

void QLogListener::setFilterLevel(Level level) {
    LOG(logger_, DEBUG) << "Updating filter level to " << to_string(level);
    filter_level_ = level;
    UpdateDisplayed();
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
        UpdateDisplayed();
        return true;
    }
    return false;
}

bool QLogListener::setFilterTopic(const std::string& topic) {
    if(filter_topic_list_.contains(topic)) {
        LOG(logger_, DEBUG) << "Updating filter topic to " << topic;
        filter_topic_ = topic;
        UpdateDisplayed();
        return true;
    }
    return false;
}

void QLogListener::setFilterMessage(const QString& pattern) {
    LOG(logger_, DEBUG) << "Updating filter pattern for message to " << pattern.toStdString();
    filter_message_.setPattern(pattern);
    UpdateDisplayed();
}
