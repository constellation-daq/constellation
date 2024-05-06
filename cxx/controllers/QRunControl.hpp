#include <QAbstractListModel>

#include <array>
#include <memory>

#include "constellation/controller/Controller.hpp"

class QRunControl : public QAbstractListModel, public constellation::controller::Controller {
    Q_OBJECT
public:
    QRunControl(std::string_view controller_name, QObject* parent = 0);

    int rowCount(const QModelIndex& /*unused*/) const override { return connections_.size(); }

    int columnCount(const QModelIndex& /*unused*/) const override { return headers_.size(); }

    QVariant data(const QModelIndex& index, int role) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void initialize(const QModelIndex& index);
    void launch(const QModelIndex& index);
    void land(const QModelIndex& index);
    void reconfigure(const QModelIndex& index);
    void start(const QModelIndex& index);
    void stop(const QModelIndex& index);

private:
    static constexpr std::array<std::string, 6> headers_ {"type", "name", "state", "connection", "message", "information"};
};
