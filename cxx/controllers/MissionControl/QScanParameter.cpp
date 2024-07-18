#include "QScanParameter.hpp"

#include <string>
#include <vector>

using namespace constellation::config;

void QScanParameter::add(const std::string& satellite, const std::string& parameter, const std::vector<Value>& values) {
    beginInsertRows(QModelIndex(), 0, 0);
    parameters_.emplace_back(satellite, parameter, values);
    endInsertRows();
}

void QScanParameter::clear() {
    beginRemoveRows(QModelIndex(), 0, parameters_.size() - 1);
    parameters_.clear();
    endRemoveRows();
}

QVariant QScanParameter::data(const QModelIndex& index, int role) const {

    if(role != Qt::DisplayRole || !index.isValid()) {
        return QVariant();
    }

    if(index.row() >= static_cast<int>(parameters_.size()) || index.column() >= static_cast<int>(headers_.size())) {
        return QVariant();
    }

    // Select connection by index:
    auto it = parameters_.begin();
    std::advance(it, index.row());

    switch(index.column()) {
    case 0: {
        return QString::fromStdString(std::get<0>(*it));
    }
    case 1: {
        return QString::fromStdString(std::get<1>(*it));
    }
    case 2: {
        return static_cast<uint>(std::get<2>(*it).size());
    }
    case 3: {
        std::stringstream s;
        for(const auto& v : std::get<2>(*it)) {
            s << v.str() << ",";
        }
        return QString::fromStdString(s.str());
    }
    default: {
        return QString("");
    }
    }
}

QVariant QScanParameter::headerData(int section, Qt::Orientation orientation, int role) const {
    if(role != Qt::DisplayRole)
        return QVariant();

    if(orientation == Qt::Horizontal && section < static_cast<int>(headers_.size())) {
        return QString::fromStdString(headers_[section]);
    }
    return QVariant();
}
