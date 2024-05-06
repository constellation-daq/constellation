#include "QRunControl.hpp"

#include <string>
#include <vector>

#include <qmetatype.h>

using namespace constellation::controller;

QRunControl::QRunControl(std::string_view controller_name, QObject* parent)
    : QAbstractListModel(parent), Controller(controller_name) {}

QVariant QRunControl::data(const QModelIndex& index, int role) const {

    if(role != Qt::DisplayRole || !index.isValid()) {
        return QVariant();
    }

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
        const auto name_endpos = name.find_first_of('.', 0);
        return QString::fromStdString(name.substr(name_endpos));
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

void QRunControl::initialize(const QModelIndex& index) {
    // Select connection by index:
    auto it = connections_.begin();
    std::advance(it, index.row());
    Controller::initialize(it->first);
}

void QRunControl::launch(const QModelIndex& index) {
    // Select connection by index:
    auto it = connections_.begin();
    std::advance(it, index.row());
    Controller::launch(it->first);
}

void QRunControl::land(const QModelIndex& index) {
    // Select connection by index:
    auto it = connections_.begin();
    std::advance(it, index.row());
    Controller::land(it->first);
}

void QRunControl::reconfigure(const QModelIndex& index) {
    // Select connection by index:
    auto it = connections_.begin();
    std::advance(it, index.row());
    Controller::reconfigure(it->first);
}

void QRunControl::start(const QModelIndex& index) {
    // Select connection by index:
    auto it = connections_.begin();
    std::advance(it, index.row());
    Controller::start(it->first);
}

void QRunControl::stop(const QModelIndex& index) {
    // Select connection by index:
    auto it = connections_.begin();
    std::advance(it, index.row());
    Controller::stop(it->first);
}
