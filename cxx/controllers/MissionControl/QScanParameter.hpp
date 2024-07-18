#include <QAbstractListModel>
#include <QSortFilterProxyModel>

#include <array>
#include <memory>
#include <optional>
#include <vector>

#include "constellation/core/config/Value.hpp"

class QScanParameter : public QAbstractListModel {

    Q_OBJECT

public:
    QScanParameter(QObject* parent = 0) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& /*unused*/) const override { return parameters_.size(); }

    int columnCount(const QModelIndex& /*unused*/) const override { return headers_.size(); }

    QVariant data(const QModelIndex& index, int role) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void add(const std::string& satellite,
             const std::string& parameter,
             const std::vector<constellation::config::Value>& values);

    void clear();

private:
    static constexpr std::array<const char*, 4> headers_ {"Satellite", "Parameter", "Steps", "Values"};

    std::vector<std::tuple<std::string, std::string, std::vector<constellation::config::Value>>> parameters_;
};
