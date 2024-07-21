#include "QRunControl.hpp"

#include <string>
#include <vector>

#include <qmetatype.h>

#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::config;
using namespace constellation::controller;
using namespace constellation::protocol;

QRunControl::QRunControl(std::string_view controller_name, QObject* parent)
    : QAbstractListModel(parent), Controller(controller_name) {}

QVariant QRunControl::data(const QModelIndex& index, int role) const {

    if(role != Qt::DisplayRole || !index.isValid()) {
        return QVariant();
    }

    const std::lock_guard connection_lock {connection_mutex_};

    if(index.row() >= static_cast<int>(connections_.size()) || index.column() >= static_cast<int>(headers_.size())) {
        return QVariant();
    }

    // Select connection by index:
    auto it = connections_.begin();
    std::advance(it, index.row());

    auto& name = it->first;
    auto& conn = it->second;

    switch(index.column()) {
    case 0: {
        const auto type_endpos = name.find_first_of('.', 0);
        return QString::fromStdString(name.substr(0, type_endpos));
    }
    case 1: {
        const auto name_startpos = name.find_first_of('.', 0);
        return QString::fromStdString(name.substr(name_startpos + 1));
    }
    case 2: {
        const auto state = std::string(magic_enum::enum_name(conn.state));
        return QString::fromStdString(state);
    }
    case 3: {
        const std::string last_endpoint = conn.req.get(zmq::sockopt::last_endpoint);
        return QString::fromStdString(last_endpoint);
    }
    case 4: {
        return QString::fromStdString(constellation::utils::to_string(conn.last_cmd_type));
    }
    case 5: {
        return QString::fromStdString(conn.last_cmd_verb);
    }
    default: {
        return QString("");
    }
    }
}

QVariant QRunControl::headerData(int section, Qt::Orientation orientation, int role) const {
    if(role != Qt::DisplayRole)
        return QVariant();

    if(orientation == Qt::Horizontal && section >= 0 && section < static_cast<int>(headers_.size())) {
        return QString::fromStdString(headers_[section]);
    }
    return QVariant();
}

void QRunControl::reached_state(CSCP::State state) {
    emit reachedGlobalState(state);
}

void QRunControl::propagate_update(std::size_t position) {
    emit dataChanged(createIndex(0, 0), createIndex(position, headers_.size() - 1));
}

void QRunControl::prepare_update(bool added, std::size_t position) {
    if(added) {
        beginInsertRows(QModelIndex(), position, position);
    } else {
        beginRemoveRows(QModelIndex(), position, position);
    }
}

void QRunControl::finalize_update(bool added, std::size_t connections) {
    if(added) {
        endInsertRows();
    } else {
        endRemoveRows();
    }

    // Mark entire data as changed:
    emit dataChanged(createIndex(0, 0), createIndex(connections - 1, headers_.size() - 1));

    // Emit signal for changed connections
    emit connectionsChanged(connections);
}

Dictionary QRunControl::getQCommands(const QModelIndex& index) {
    std::unique_lock<std::mutex> lock(connection_mutex_);

    // Select connection by index:
    auto it = connections_.begin();
    std::advance(it, index.row());

    // Unlock so the controller can grab it
    lock.unlock();
    auto msg = Controller::sendCommand(it->first, "get_commands");

    // FIXME check success
    return Dictionary::disassemble(msg.getPayload());
}

std::string QRunControl::getQName(const QModelIndex& index) const {
    std::unique_lock<std::mutex> lock(connection_mutex_);

    // Select connection by index:
    auto it = connections_.begin();
    std::advance(it, index.row());

    return it->first;
}

std::optional<std::string> QRunControl::sendQCommand(const QModelIndex& index,
                                                     const std::string& verb,
                                                     const CommandPayload& payload) {
    std::unique_lock<std::mutex> lock(connection_mutex_);

    // Select connection by index:
    auto it = connections_.begin();
    std::advance(it, index.row());

    // Unlock so the controller can grab it
    lock.unlock();

    const auto& msg = Controller::sendCommand(it->first, verb, payload);
    const auto& response = msg.getPayload();

    if(!response.empty()) {
        try {
            return Dictionary::disassemble(response).to_string();
        } catch(msgpack::type_error&) {
            return std::string(response.to_string_view());
        }
    }

    return {};
}

bool QRunControlSortProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const {

    QVariant leftData = sourceModel()->data(left);
    QVariant rightData = sourceModel()->data(right);

    QString leftString = leftData.toString();
    QString rightString = rightData.toString();
    return QString::localeAwareCompare(leftString, rightString) < 0;
}
