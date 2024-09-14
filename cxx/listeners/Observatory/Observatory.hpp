/**
 * @file
 * @brief Observatory GUI
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <QCloseEvent>
#include <QItemDelegate>
#include <QMainWindow>
#include <QMessageBox>
#include <QPainter>
#include <QSettings>

#include "LogDialog.hpp"
#include "QLogListener.hpp"
#include "ui_Observatory.h"

class LogItemDelegate : public QItemDelegate {
public:
    LogItemDelegate(QLogListener* model);

private:
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
    QLogListener* log_listener_;

    const std::map<constellation::log::Level, QColor> level_colors {
        {constellation::log::Level::TRACE, QColor(224, 224, 224, 128)},
        {constellation::log::Level::DEBUG, QColor(200, 200, 200, 128)},
        {constellation::log::Level::INFO, QColor(191, 191, 191, 128)},
        {constellation::log::Level::WARNING, QColor(255, 138, 0, 128)},
        {constellation::log::Level::STATUS, QColor(0, 100, 0, 128)},
        {constellation::log::Level::CRITICAL, QColor(255, 0, 0, 128)},
        {constellation::log::Level::OFF, QColor(0, 0, 0, 128)},
    };
};

class Observatory : public QMainWindow, public Ui::wndLog {
    Q_OBJECT
public:
    Observatory(std::string_view group_name);
    virtual ~Observatory();

public:
    void closeEvent(QCloseEvent*) override;

private slots:
    /**
     * @brief Private slot to update the scroll position to the newly inserted and displayed message
     * @param i model index to which to scroll
     */
    void new_message_display(const QModelIndex& i);

    void on_globalLevel_currentIndexChanged(int index);
    void on_filterLevel_currentIndexChanged(int index);
    void on_filterSender_currentTextChanged(const QString& text);
    void on_filterTopic_currentTextChanged(const QString& text);
    void on_filterMessage_editingFinished();
    void on_clearFilters_clicked();

    void on_viewLog_activated(const QModelIndex& i);

private:
    static void CheckRegistered();
    QLogListener log_listener_;
    LogItemDelegate log_message_delegate_;

    /** UI Settings */
    QSettings gui_settings_;
};
