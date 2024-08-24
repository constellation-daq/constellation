#include <array>
#include <memory>
#include <optional>
#include <QAbstractListModel>
#include <QSortFilterProxyModel>

#include "constellation/controller/Controller.hpp"

class QController : public QAbstractListModel, public constellation::controller::Controller {

    Q_OBJECT

public:
    QController(std::string controller_name, QObject* parent = 0);

    int rowCount(const QModelIndex& /*unused*/) const override { return connections_.size(); }

    int columnCount(const QModelIndex& /*unused*/) const override { return headers_.size(); }

    QVariant data(const QModelIndex& index, int role) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    std::optional<std::string> sendQCommand(const QModelIndex& index,
                                            const std::string& verb,
                                            const CommandPayload& payload = {});

    constellation::config::Dictionary getQCommands(const QModelIndex& index);

    std::string getQName(const QModelIndex& index) const;

signals:
    void connectionsChanged(std::size_t connections);
    void reachedGlobalState(constellation::protocol::CSCP::State state);
    void reachedLowestState(constellation::protocol::CSCP::State state);

protected:
    void reached_global_state(constellation::protocol::CSCP::State state) override;
    void reached_lowest_state(constellation::protocol::CSCP::State state) override;
    void propagate_update(Controller::UpdateType type, std::size_t position, std::size_t total) override;

private:
    static constexpr std::array<const char*, 6> headers_ {
        "Type", "Name", "State", "Connection", "Last response", "Last message"};

    std::size_t current_rows_ {0};
};

class QControllerSortProxy : public QSortFilterProxyModel {
    Q_OBJECT

public:
    QControllerSortProxy(QObject* parent = nullptr);

    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};
