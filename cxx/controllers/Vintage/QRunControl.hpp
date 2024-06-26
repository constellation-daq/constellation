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

    void sendQCommand(const QModelIndex& index, const std::string& verb, const CommandPayload& payload = {});

    constellation::config::Dictionary getQCommands(const QModelIndex& index);

protected:
    void propagate_update(std::size_t connections) override;

private:
    static constexpr std::array<const char*, 6> headers_ {
        "type", "name", "state", "connection", "last response", " last message"};

    std::size_t current_rows_ {0};
};
