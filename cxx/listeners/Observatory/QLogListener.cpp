#include "QLogListener.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

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
    case 0:
    case 1: return 100;
    case 2: return 60;
    case 3: return 400;
    default: return -1;
    }
}

QString LogMessage::operator[](int i) const {
    return Text(i).c_str();
}

std::string LogMessage::Text(int i) const {
    switch(i) {
    case 0: return to_string(getHeader().getTime());
    case 1: return std::string(getHeader().getSender());
    case 2: return to_string(getLogLevel());
    case 3: return std::string(getLogTopic());
    case 4: return std::string(getLogMessage());
    case 5: return "";
    case 6: return "";
    default: return "";
    }
}

QString LogMessage::ColumnName(int i) {
    if(i < 0 || i >= static_cast<int>(headers_.size())) {
        return {};
    }
    return headers_[i];
}

LogSearcher::LogSearcher() : m_set(false) {
    // FIXME port for Qt6
    // m_regexp.setPattern(QRegularExpression::Wildcard);
    // m_regexp.setCaseSensitivity(Qt::CaseInsensitive);
}

void LogSearcher::SetSearch(const std::string& regexp) {
    m_set = (regexp != "");
    m_regexp.setPattern(regexp.c_str());
}

bool LogSearcher::Match(const LogMessage&) {
    if(!m_set)
        return true;
    for(int i = 0; i < LogMessage::NumColumns(); ++i) {
        // if (m_regexp.index In(msg[i]) >= 0)
        // return true;
    }
    return false;
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
    : QAbstractListModel(parent),
      SubscriberPool<CMDP1LogMessage, MONITORING>(
          "LOGRECV", [this](auto&& arg) { add_message(std::forward<decltype(arg)>(arg)); }, {"LOG"}),
      logger_("QLGRCV"), m_displaylevel(Level::WARNING), m_sorter(&m_all) {}

bool QLogListener::IsDisplayed(size_t index) {
    LogMessage& msg = m_all[index];
    return ((msg.getLogLevel() >= m_displaylevel && (m_displaytype == "" || m_displaytype == "All")) ||
            ((m_displayname == "" || m_displayname == "*" || msg.getHeader().getSender() == m_displayname) &&
             m_search.Match(msg)));
}

void QLogListener::add_message(CMDP1LogMessage&& msg) {

    LOG(logger_, INFO) << "Recv msg";
    m_all.emplace_back(std::move(msg));
    if(IsDisplayed(m_all.size() - 1)) {
        std::vector<size_t>::iterator it = std::lower_bound(m_disp.begin(), m_disp.end(), m_all.size() - 1, m_sorter);
        size_t pos = it - m_disp.begin();
        beginInsertRows(QModelIndex(), pos, pos);
        m_disp.insert(it, m_all.size() - 1);
        endInsertRows();
        // return createIndex(pos, 0); // FIXME we need to tell someone that we changed it...?
    }
    // return QModelIndex(); // FIXME we need to tell someone that we changed it...?
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

void QLogListener::SetSearch(const std::string& regexp) {
    m_search.SetSearch(regexp);
    UpdateDisplayed();
}

void QLogListener::SetDisplayNames(const std::string& type, const std::string& name) {
    m_displaytype = type;
    m_displayname = name;
    UpdateDisplayed();
}

void QLogListener::SetDisplayLevel(Level level) {
    m_displaylevel = level;
    UpdateDisplayed();
}
