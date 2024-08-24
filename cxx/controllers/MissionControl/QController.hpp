/**
 * @file
 * @brief QController definition
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <array>
#include <memory>
#include <optional>
#include <QAbstractListModel>
#include <QSortFilterProxyModel>

#include "constellation/controller/Controller.hpp"

/**
 * @class QController
 * @brief Qt Controller base class providing an interface to \ref constellation::controller::Controller
 *
 * @details This class provides a thin wrapper around \ref constellation::controller::Controller instances to allow using
 * it in Qt graphical user interfaces, e.g. for displaying connections in a QTreeView. It overwrites the notification
 * methods and emits signals for their calls and allows querying connection details via QModelIndex.
 *
 */
class QController : public QAbstractListModel, public constellation::controller::Controller {
    Q_OBJECT

public:
    /**
     * @brief Construct a QController base object
     *
     * @param controller_name Name of the controller
     * @param parent Parent Qt object
     */
    QController(std::string controller_name, QObject* parent = 0);

    /**
     * @brief Get total number of rows, i.e. number of connections the controller has
     *
     * @param index QModelIndex, unused
     * @return Number of connections
     */
    int rowCount(const QModelIndex& index) const override;

    /**
     * @brief Get the (fixed) number of columns of this QAsbtractListModel, providing connection details
     *
     * @param index QModelIndex, unused
     * @return Number of columns
     */
    int columnCount(const QModelIndex& index) const override;

    /**
     * @brief Retrieve the data of a givel cell (column, row) of the model i.e. a specific connection detail
     *
     * \param index QModelIndex to obtain the data for
     * \param role Role code
     *
     * \return QVariant holding the connection detail data for the requested cell
     */
    QVariant data(const QModelIndex& index, int role) const override;

    /**
     * @brief Retrieve the header information (connection details) for a given column
     *
     * @param column Column of the model to retrieve the header title for
     * @param orientation Orientation of the UI
     * @param role ROle code
     * @return Header title of the requested column
     */
    QVariant headerData(int column, Qt::Orientation orientation, int role) const override;

    /**
     * @brief Helper method to send commands to a single satellite
     * @details This method allows sending commands via the \ref constellation::controller::Controller by addressing the
     * satellite in question via its QModelIndex instead of its name. This can be used e.g. for context menu actions.
     *
     * @param index QModelIndex of the satellite in question
     * @param verb Command verb to be sent
     * @param payload Optional payload to be attached to the command
     * @return Optional satellite response from the CSCP verb
     */
    std::optional<std::string> sendQCommand(const QModelIndex& index,
                                            const std::string& verb,
                                            const CommandPayload& payload = {});

    /**
     * @brief Helper to obtain the list of available commands for a single satellite
     * @details This method allows requesting the commands by addressing the satellite in question via its QModelIndex
     * instead of its name. This can be used e.g. for context menu actions.
     *
     * @param index QModelIndex of the satellite in question
     * @return Dictionary with commands as keys and descriptions as values
     */
    constellation::config::Dictionary getQCommands(const QModelIndex& index);

    /**
     * @brief Helper to obtain the name of a satellite identified by its QModelIndex
     *
     * @param index QModelIndex of the satellite in question
     * @return Canonical name of the satellite
     */
    std::string getQName(const QModelIndex& index) const;

signals:
    /**
     * @brief Signal emitted whenever a connection changed
     * @param connections Number of currently held connections
     */
    void connectionsChanged(std::size_t connections);

    /**
     * @brief Signal emitted whenever the Constellation enters a global state
     *
     * @param state State the Constellation entered into
     */
    void reachedGlobalState(constellation::protocol::CSCP::State state);

    /**
     * @brief Signal emitted whenever the state of the Constellation changed but the resulting situation is not a global
     * state
     *
     * @param state Lowest state the Constellation currently includes
     */
    void reachedLowestState(constellation::protocol::CSCP::State state);

protected:
    /**
     * @brief Helper method emitting the reachedGlobalState signal
     *
     * @param state State the Constellation entered into
     */
    void reached_global_state(constellation::protocol::CSCP::State state) override;

    /**
     * @brief Helper method emitting the reachedLowestState signal
     *
     * @param state Lowest state the Constellation currently includes
     */
    void reached_lowest_state(constellation::protocol::CSCP::State state) override;

    /**
     * @brief Helper method emitting the connectionsChanged signal
     *
     * @param type Type of update the controller has performed
     * @param position Position of the changed connections in the connection list
     * @param total Total number of current connections
     */
    void propagate_update(Controller::UpdateType type, std::size_t position, std::size_t total) override;

private:
    // Column headers of the connection details
    static constexpr std::array<const char*, 6> headers_ {
        "Type", "Name", "State", "Connection", "Last response", "Last message"};
};

class QControllerSortProxy : public QSortFilterProxyModel {
    Q_OBJECT

public:
    QControllerSortProxy(QObject* parent = nullptr);

    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};
