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

static const int alpha = 96;
static QColor level_colours[] = {
    QColor(224, 224, 224, alpha), // DEBUG
    QColor(192, 255, 192, alpha), // OK
    QColor(255, 224, 224, alpha), // THROW
    QColor(192, 208, 255, alpha), // EXTRA
    QColor(255, 255, 192, alpha), // INFO
    QColor(255, 224, 96, alpha),  // WARN
    QColor(255, 96, 96, alpha),   // ERROR
    QColor(208, 96, 255, alpha),  // USER
    QColor(192, 255, 192, alpha), // BUSY
    QColor(192, 192, 255, alpha), // NONE
};

class LogItemDelegate : public QItemDelegate {
public:
    LogItemDelegate(QLogListener* model);

private:
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
    QLogListener* m_model;
};

class Observatory : public QMainWindow, public Ui::wndLog {
    Q_OBJECT
public:
    Observatory(std::string_view group_name);
    virtual ~Observatory();

public:
    void closeEvent(QCloseEvent*) override;
private slots:
    void on_cmbLevel_currentIndexChanged(int index);
    void on_cmbFrom_currentIndexChanged(const QString& text);
    void on_txtSearch_editingFinished();
    void on_viewLog_activated(const QModelIndex& i);

private:
    static void CheckRegistered();
    QLogListener m_model;
    LogItemDelegate m_delegate;

    /** UI Settings */
    QSettings gui_settings_;
};
