/**
 * @file
 * @brief QController implementation
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QController.hpp"

#include <cstddef>
#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

#include <zmq.hpp>

#include <QAbstractListModel>
#include <QMetaType>
#include <QSortFilterProxyModel>
#include <Qt>

#include "constellation/controller/Controller.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::config;
using namespace constellation::controller;
using namespace constellation::message;
using namespace constellation::protocol;
using namespace constellation::utils;

QController::QController(std::string controller_name, QObject* parent)
    : QAbstractListModel(parent), Controller(std::move(controller_name)) {}

int QController::rowCount(const QModelIndex& /*unused*/) const {
    return static_cast<int>(getConnectionCount());
}

int QController::columnCount(const QModelIndex& /*unused*/) const {
    return static_cast<int>(headers_.size());
}

QVariant QController::data(const QModelIndex& index, int role) const {

    if(role != Qt::DisplayRole || !index.isValid()) {
        return {};
    }

    if(index.row() >= static_cast<int>(getConnectionCount()) || index.column() >= static_cast<int>(headers_.size())) {
        return {};
    }

    const std::lock_guard connection_lock {connection_mutex_};
    // Select connection by index:
    auto it = connections_.begin();
    std::advance(it, index.row());

    const auto& name = it->first;
    const auto& conn = it->second;

    switch(index.column()) {
    case 0: {
        // Satellite type
        const auto type_endpos = name.find_first_of('.', 0);
        return QString::fromStdString(name.substr(0, type_endpos));
    }
    case 1: {
        // Satellite name
        const auto name_startpos = name.find_first_of('.', 0);
        return QString::fromStdString(name.substr(name_startpos + 1));
    }
    case 2: {
        // State
        return getStyledState(conn.state, true);
    }
    case 3: {
        // Connection (URI)
        const std::string last_endpoint = conn.req.get(zmq::sockopt::last_endpoint);
        return QString::fromStdString(last_endpoint);
    }
    case 4: {
        // Last command response type
        return getStyledResponse(conn.last_cmd_type);
    }
    case 5: {
        // Last command response message
        return QString::fromStdString(conn.last_cmd_verb);
    }
    case 6: {
        // Heartbeat period
        return QString::fromStdString(to_string(conn.interval));
    }
    case 7: {
        // Remaining lives:
        return conn.lives;
    }
    default: {
        return QString("");
    }
    }
}

QVariant QController::headerData(int column, Qt::Orientation orientation, int role) const {
    if(role == Qt::DisplayRole && orientation == Qt::Horizontal && column >= 0 &&
       column < static_cast<int>(headers_.size())) {
        return QString::fromStdString(headers_.at(column));
    }
    return {};
}

QString QController::getStyledState(CSCP::State state, bool global) {

    const QString global_indicatior = (global ? "" : " ≊");

    switch(state) {
    case CSCP::State::NEW: {
        return "<font color='gray'><b>New</b>" + global_indicatior + "</font>";
    }
    case CSCP::State::initializing: {
        return "<font color='gray'><b>Initializing...</b>" + global_indicatior + "</font>";
    }
    case CSCP::State::INIT: {
        return "<font color='gray'><b>Initialized</b>" + global_indicatior + "</font>";
    }
    case CSCP::State::launching: {
        return "<font color='orange'><b>Launching...</b>" + global_indicatior + "</font>";
    }
    case CSCP::State::landing: {
        return "<font color='orange'><b>Landing...</b>" + global_indicatior + "</font>";
    }
    case CSCP::State::reconfiguring: {
        return "<font color='orange'><b>Reconfiguring...</b>" + global_indicatior + "</font>";
    }
    case CSCP::State::ORBIT: {
        return "<font color='orange'><b>Orbiting</b>" + global_indicatior + "</font>";
    }
    case CSCP::State::starting: {
        return "<font color='green'><b>Starting...</b>" + global_indicatior + "</font>";
    }
    case CSCP::State::stopping: {
        return "<font color='green'><b>Stopping...</b>" + global_indicatior + "</font>";
    }
    case CSCP::State::RUN: {
        return "<font color='green'><b>Running</b>" + global_indicatior + "</font>";
    }
    case CSCP::State::SAFE: {
        return "<font color='red'><b>Safe Mode</b>" + global_indicatior + "</font>";
    }
    case CSCP::State::interrupting: {
        return "<font color='red'><b>Interrupting...</b>" + global_indicatior + "</font>";
    }
    case CSCP::State::ERROR: {
        return "<font color='darkred'><b>Error</b>" + global_indicatior + "</font>";
    }
    default: std::unreachable();
    }
}

QString QController::getStyledResponse(CSCP1Message::Type type) {

    const auto type_string = QString::fromStdString(constellation::utils::to_string(type));
    switch(type) {
    case CSCP1Message::Type::REQUEST:
    case CSCP1Message::Type::NOTIMPLEMENTED: {
        return "<font color='gray'>New</b>" + type_string + "</font>";
    }
    case CSCP1Message::Type::SUCCESS: {
        return "<font color='green'>" + type_string + "</font>";
    }
    case CSCP1Message::Type::INCOMPLETE:
    case CSCP1Message::Type::INVALID:
    case CSCP1Message::Type::UNKNOWN: {
        return "<font color='orange'>" + type_string + "</font>";
    }
    case CSCP1Message::Type::ERROR: {
        return "<font color='darkred'>" + type_string + "</font>";
    }
    default: std::unreachable();
    }
}

void QController::reached_state(CSCP::State state, bool global) {
    LOG(logger_, DEBUG) << "Reached new " << (global ? "global" : "lowest") << " state " << to_string(state);
    emit reachedState(state, global);
}

void QController::propagate_update(UpdateType type, std::size_t position, std::size_t total) {
    if(type == UpdateType::ADDED) {
        beginInsertRows(QModelIndex(), static_cast<int>(position), static_cast<int>(position));
        endInsertRows();
        // Emit signal for changed connections
        emit connectionsChanged(total);
    } else if(type == UpdateType::REMOVED) {
        beginRemoveRows(QModelIndex(), static_cast<int>(position), static_cast<int>(position));
        endRemoveRows();

        // Emit signal for changed connections
        emit connectionsChanged(total);
    }

    emit dataChanged(createIndex(static_cast<int>(position), 0),
                     createIndex(static_cast<int>(position), headers_.size() - 1));
}

Dictionary QController::getQCommands(const QModelIndex& index) {
    const std::lock_guard lock {connection_mutex_};

    // Select connection by index:
    auto it = connections_.begin();
    std::advance(it, index.row());

    return it->second.commands;
}

std::string QController::getQName(const QModelIndex& index) const {
    const std::lock_guard lock {connection_mutex_};

    // Select connection by index:
    auto it = connections_.begin();
    std::advance(it, index.row());

    return it->first;
}

std::optional<std::string> QController::sendQCommand(const QModelIndex& index,
                                                     const std::string& verb,
                                                     const CommandPayload& payload) {
    std::unique_lock<std::mutex> lock {connection_mutex_};

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
            try {
                return List::disassemble(response).to_string();
            } catch(msgpack::type_error&) {
                try {
                    return Value::disassemble(response).str();
                } catch(msgpack::type_error&) {
                    return std::string(response.to_string_view());
                }
            }
        }
    }

    emit dataChanged(createIndex(index.row(), 0), createIndex(index.row(), headers_.size() - 1));
    return {};
}

std::map<std::string, CSCP1Message> QController::sendQCommands(std::string verb, const CommandPayload& payload) {
    auto replies = sendCommands(std::move(verb), payload);

    emit dataChanged(createIndex(0, 0), createIndex(static_cast<int>(getConnectionCount() - 1), headers_.size() - 1));
    return replies;
}

std::map<std::string, CSCP1Message> QController::sendQCommands(const std::string& verb,
                                                               const std::map<std::string, CommandPayload>& payloads) {
    auto replies = sendCommands(verb, payloads);

    emit dataChanged(createIndex(0, 0), createIndex(static_cast<int>(getConnectionCount() - 1), headers_.size() - 1));
    return replies;
}

QControllerSortProxy::QControllerSortProxy(QObject* parent) : QSortFilterProxyModel(parent) {}

bool QControllerSortProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const {

    const QVariant leftData = sourceModel()->data(left);
    const QVariant rightData = sourceModel()->data(right);

    const QString leftString = leftData.toString();
    const QString rightString = rightData.toString();
    return QString::localeAwareCompare(leftString, rightString) < 0;
}
