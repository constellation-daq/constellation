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
#include <QMainWindow>
#include <QMessageBox>
#include <QPainter>
#include <QSettings>
#include <QStyledItemDelegate>

#include "LogDialog.hpp"
#include "QLogFilter.hpp"
#include "QLogListener.hpp"
#include "ui_Observatory.h"

class LogItemDelegate : public QStyledItemDelegate {
public:
    LogItemDelegate() = default;

    QString displayText(const QVariant& value, const QLocale& locale) const override;

private:
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    static inline std::map<constellation::log::Level, QColor> level_colors {
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
    void closeEvent(QCloseEvent* /*event*/) override;

private slots:
    void on_globalLevel_currentIndexChanged(int index);
    void on_filterLevel_currentIndexChanged(int index);
    void on_filterSender_currentTextChanged(const QString& text);
    void on_filterTopic_currentTextChanged(const QString& text);
    void on_filterMessage_editingFinished();
    void on_clearFilters_clicked();

    void on_viewLog_activated(const QModelIndex& i);

private:
    QLogListener log_listener_;
    QLogFilter log_filter_;
    LogItemDelegate log_message_delegate_;

    /** UI Settings */
    QSettings gui_settings_;
};
