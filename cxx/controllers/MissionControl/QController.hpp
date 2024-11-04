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
#include <cstddef>
#include <map>
#include <optional>
#include <string>

#include <QAbstractListModel>
#include <QObject>
#include <QSortFilterProxyModel>
#include <Qt>
#include <QVariant>

#include "constellation/controller/Controller.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"

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
    QController(std::string controller_name, QObject* parent = nullptr);

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
     * @param index QModelIndex to obtain the data for
     * @param role Role code
     *
     * @return QVariant holding the connection detail data for the requested cell
     */
    QVariant data(const QModelIndex& index, int role) const override;

    /**
     * @brief Retrieve the header information (connection details) for a given column
     *
     * @param column Column of the model to retrieve the header title for
     * @param orientation Orientation of the UI
     * @param role Role code
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
     * @brief Send a command to all connected satellites
     * @details This method allows to send command message to all connected satellites. The message is formed from the
     * provided verb and optional payload. The payload is the same for all satellites. The response from all satellites
     * is returned as a map. After the responses have been collected, a dataChanged signal is emitted.
     *
     * @param verb Command
     * @param payload Optional payload for this command message
     *
     * @return Map of satellite canonical names and their CSCP response messages
     */
    std::map<std::string, constellation::message::CSCP1Message> sendQCommands(std::string verb,
                                                                              const CommandPayload& payload = {});

    /**
     * @brief Send a command to all connected satellites
     * @details This method allows to send command message to all connected satellites. The message is formed
     * individually for each satellite from the provided verb and the payload entry in the map for the given satellite.
     * Missing entries in the payload table will receive an empty payload. The response from all satellites is
     * returned as a map. After the responses have been collected, a dataChanged signal is emitted.
     *
     * @param verb Command
     * @param payloads Map of payloads for each target satellite.
     *
     * @return Map of satellite canonical names and their CSCP response messages
     */
    std::map<std::string, constellation::message::CSCP1Message>
    sendQCommands(const std::string& verb, const std::map<std::string, CommandPayload>& payloads);

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

    /**
     * @brief Helper to obtain the state string with color and formatting
     *
     * @param state State to obtain string for
     * @param global Marker if the state is global or not
     *
     * @return String for the state display
     */
    static QString getStyledState(constellation::protocol::CSCP::State state, bool global);

    /**
     * @brief Helper to obtain the CSCP message type string with color and formatting
     *
     * @param type CSCP message type
     *
     * @return String for the CSCP response display
     */
    static QString getStyledResponse(constellation::message::CSCP1Message::Type type);

signals:
    /**
     * @brief Signal emitted whenever a connection changed
     * @param connections Number of currently held connections
     */
    void connectionsChanged(std::size_t connections);

    /**
     * @brief Signal emitted whenever the state of the Constellation changed, either to a new lowest or a new global state
     *
     * @param state State the Constellation entered into
     * @param global Boolean indicating whether this is a global or a lowest state
     */
    void reachedState(constellation::protocol::CSCP::State state, bool global);

protected:
    /**
     * @brief Helper method emitting the reachedGlobalState and reachedLowestSate signals
     *
     * @param state Global or lowest state the Constellation entered into
     * @param global Flag whether the state is a new global or lowest state
     */
    void reached_state(constellation::protocol::CSCP::State state, bool global) final;

    /**
     * @brief Helper method emitting the connectionsChanged signal
     *
     * @param type Type of update the controller has performed
     * @param position Position of the changed connections in the connection list
     * @param total Total number of current connections
     */
    void propagate_update(Controller::UpdateType type, std::size_t position, std::size_t total) final;

private:
    // Column headers of the connection details
    static constexpr std::array<const char*, 8> headers_ {
        "Type", "Name", "State", "Connection", "Last response", "Last message", "Heartbeat", "Lives"};
};

class QControllerSortProxy : public QSortFilterProxyModel {
    Q_OBJECT

public:
    QControllerSortProxy(QObject* parent = nullptr);

    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};
