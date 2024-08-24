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
#include <QTimer>

#include "QController.hpp"
#include "ui_MissionControl.h"

class MissionControl : public QMainWindow, public Ui::wndRun {

    Q_OBJECT
public:
    MissionControl(std::string controller_name, std::string_view group_name);

private:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void update_run_infos();

    void update_run_identifier(const QString& text, int number);

    void on_btnInit_clicked();
    void on_btnLand_clicked();
    void on_btnConfig_clicked();
    void on_btnStart_clicked();
    void on_btnStop_clicked();
    void on_btnShutdown_clicked();
    void on_btnLog_clicked();
    void on_btnLoadConf_clicked();
    void onCustomContextMenu(const QPoint& point);

private:
    std::map<std::string, constellation::controller::Controller::CommandPayload> parseConfigFile(QString file);
    constellation::controller::Controller::CommandPayload parseConfigFile(QString file, const QModelIndex& index);

    QController runcontrol_;
    QControllerSortProxy sorting_proxy_;
    constellation::log::Logger logger_;
    constellation::log::Logger user_logger_;

    /* Run identifier */
    QString current_run_;
    QDateTime run_start_time_;

    QString get_state_str(constellation::protocol::CSCP::State state, bool global_) const;
    void update_button_states(constellation::protocol::CSCP::State state);

    QTimer m_timer_display;

    QMenu* contextMenu;
    bool m_lastexit_success;

    QSettings gui_settings_;
};
