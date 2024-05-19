#include "QRunControl.hpp"

#include <string>
#include <vector>

#include "constellation/core/config/Dictionary.hpp"

#include <qmetatype.h>

using namespace constellation::config;
using namespace constellation::controller;

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
        return QString::fromStdString(conn.status);
    }
    case 5: {
        // FIXME tags?
        return QString("");
    }
    default: {
        return QString("");
    }
    }
}

QVariant QRunControl::headerData(int section, Qt::Orientation orientation, int role) const {
    if(role != Qt::DisplayRole)
        return QVariant();

    if(orientation == Qt::Horizontal && section < static_cast<int>(headers_.size())) {
        return QString::fromStdString(headers_[section]);
    }
    return QVariant();
}

void QRunControl::propagate_update(std::size_t connections) {
    if(current_rows_ < connections) {
        beginInsertRows(QModelIndex(), 0, 0);
        endInsertRows();
    } else if(current_rows_ > connections) {
        emit dataChanged(createIndex(0, 0), createIndex(connections, headers_.size() - 1));
    } else {
        emit dataChanged(createIndex(0, 0), createIndex(connections - 1, headers_.size() - 1));
    }
    current_rows_ = connections;
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

void QRunControl::sendQCommand(const QModelIndex& index, const std::string& verb, const CommandPayload& payload) {
    std::unique_lock<std::mutex> lock(connection_mutex_);

    // Select connection by index:
    auto it = connections_.begin();
    std::advance(it, index.row());

    // Unlock so the controller can grab it
    lock.unlock();

    Controller::sendCommand(it->first, verb, payload);
}
