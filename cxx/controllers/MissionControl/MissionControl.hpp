/**
 * @file
 * @brief MissionControl GUI implementation
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <QDateTime>
#include <QMainWindow>
#include <QRegularExpression>
#include <QSettings>
#include <QString>
#include <QStyledItemDelegate>
#include <QTimer>

#include "QController.hpp"
#include "ui_MissionControl.h"

class ConnectionItemDelegate : public QStyledItemDelegate {
protected:
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

/**
 * @class MissionControl
 * @brief Main window of the MissionControl UI
 * @details This class implements the Qt QMainWindow component of the MissionControl Controller UI, connects signals to
 * the slots of different UI elements and takes care of handling the run identifier. Settings corresponding to UI elements
 * are stored and retrieved again from file when restarting the UI. MissionControl is designed such that the UI can be closed
 * and reopened at any time during operating a Constellation, and the current state of the satellites is inferred from the
 * running Constellation using CSCP commands.
 */
class MissionControl : public QMainWindow, public Ui::wndRun {
    Q_OBJECT

public:
    /**
     * @brief MissionControl Constructor
     *
     * @param controller_name Name of this controller instance
     * @param group_name Constellation group name to connect to
     */
    MissionControl(std::string controller_name, std::string_view group_name);

private:
    /**
     * @brief Qt QCloseEvent handler which stores the UI settings to file
     *
     * @param event The Qt close event
     */
    void closeEvent(QCloseEvent* event) override;

private slots:
    /**
     * @brief Private slot to update run infos such as run identifier and run duration in the UI
     * @details This slot is connected to a UI timer which updates these values regularly
     */
    void update_run_infos();

    /**
     * @brief Private slot for updating the run identifier with new values.
     * @details This slot is called e.g. when the run identifier is changed from UI input elements
     *
     * @param text Run identifier text
     * @param number Run sequence
     */
    void update_run_identifier(const QString& text, int number);

    /**
     * @brief Private slot to obtain run identifier and run number from constellation when the first connection joins
     *
     * @param num Number of current connections
     */
    void startup(std::size_t num);

    /**
     * @brief Configuration file editing slot
     */
    void on_txtConfigFileName_textChanged();

    /**
     * @brief Private slot for "Init" button
     */
    void on_btnInit_clicked();

    /**
     * @brief Private slot for "Land" button
     */
    void on_btnLand_clicked();

    /**
     * @brief Private slot for "Launch" button
     */
    void on_btnConfig_clicked();

    /**
     * @brief Private slot for "Start" button
     */
    void on_btnStart_clicked();

    /**
     * @brief Private slot for "Stop" button
     */
    void on_btnStop_clicked();

    /**
     * @brief Private slot for "Shutdown" button
     */
    void on_btnShutdown_clicked();

    /**
     * @brief Private slot for "Log" button
     */
    void on_btnLog_clicked();

    /**
     * @brief Private slot for "Select" button for selecting the configuration file
     */
    void on_btnLoadConf_clicked();

    /**
     * @brief Private slot for the connection list context menu
     * @param point Selected point of the context menu
     */
    void custom_context_menu(const QPoint& point);

private:
    /**
     * @brief Helper to parse the configuration file for all satellites
     *
     * @param file File path to parse
     * @return Map of configuration dictionaries for all known satellites
     */
    std::map<std::string, constellation::controller::Controller::CommandPayload> parse_config_file(const QString& file);

    /**
     * @brief Helper to parse the configuration file for a single satellite
     *
     * @param file File path to parse
     * @param index QModelIndex of the satellite in question
     * @return Configuration as command payload for the satellite in questions
     */
    constellation::controller::Controller::CommandPayload parse_config_file(const QString& file, const QModelIndex& index);

    /** The controller instance of the UI */
    QController runcontrol_;
    ConnectionItemDelegate item_delegate_;

    /** Sorting proxy for the connection list */
    QControllerSortProxy sorting_proxy_;

    /** Logger used by the UI */
    constellation::log::Logger logger_;

    /** Logger for operator log messages */
    constellation::log::Logger user_logger_;

    /* Run identifier */
    QString current_run_;
    QDateTime run_start_time_;

    /**
     * @brief Helper to update button states
     *
     * @param state State the buttons should be in
     */
    void update_button_states(constellation::protocol::CSCP::State state);

    /** UI timer for refreshing certain elements such as the run duration */
    QTimer display_timer_;

    /** UI Settings */
    QSettings gui_settings_;
};
