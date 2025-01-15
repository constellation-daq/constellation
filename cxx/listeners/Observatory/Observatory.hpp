/**
 * @file
 * @brief Observatory GUI
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstddef>
#include <map>
#include <string_view>

#include <QCloseEvent>
#include <QDialog>
#include <QLocale>
#include <QMainWindow>
#include <QMessageBox>
#include <QModelIndex>
#include <QPainter>
#include <QSettings>
#include <QString>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QVariant>
#include <QWidget>

#include "constellation/core/log/Level.hpp"
#include "constellation/core/log/Logger.hpp"

#include "QLogFilter.hpp"
#include "QLogListener.hpp"
#include "QSenderSubscriptions.hpp"
#include "ui_Observatory.h"

class LogStatusBar : public QWidget {
public:
    LogStatusBar();
    void countMessage(constellation::log::Level level);
    void resetMessageCounts();

private:
    QHBoxLayout layout_;
    std::size_t msg_all_ {0};
    QLabel* label_all_;
    std::size_t msg_critical_ {0};
    QLabel* label_critical_;
    std::size_t msg_warning_ {0};
    QLabel* label_warning_;
};

/**
 * @class LogItemDelegate
 * @brief Delegate for drawing log items in the logging view. This adds color to the row and converts the timestamp of the
 *        log message to a format including seconds.
 */
class LogItemDelegate : public QStyledItemDelegate {
public:
    LogItemDelegate() = default;

    /**
     * @brief Convert item variant into text - returns strings and converts QDateTime into a readable timestamp with seconds
     *
     * @param value QVariant holding the value to be displayed
     * @param locale Locale of the cell
     *
     * @return String representation of the item
     */
    QString displayText(const QVariant& value, const QLocale& locale) const override;

private:
    /**
     * @brief Paint the respective view using the color of the log level severity
     *
     * @param painter Pointer to the painter object
     * @param option Style options
     * @param index QModelIndex of the item to be painted
     */
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

/**
 * @class Observatory
 * @brief Main window of the Observatory Logging UI
 * @details This class implements the Qt QMainWindow component of the Observatory Logging UI, connects signals to
 * the slots of different UI elements and takes care of updating the filter settings. Settings corresponding to UI elements
 * are stored and retrieved again from file when restarting the UI.
 */
class Observatory : public QMainWindow, public Ui::wndLog {
    Q_OBJECT
public:
    /**
     * @brief Observatory Constructor
     *
     * @param group_name Constellation group name to connect to
     */

    Observatory(std::string_view group_name);

public:
    /**
     * @brief Qt QCloseEvent handler which stores the UI settings to file
     *
     * @param event The Qt close event
     */
    void closeEvent(QCloseEvent* event) override;

private slots:

    /**
     * @brief Private slot for changes of the global subscription level setting
     *
     * @param index New index of the global subscription level
     */
    void on_globalLevel_currentIndexChanged(int index);

    /**
     * @brief Private slot for changes of the filter log level setting
     *
     * @param index New index of the filter log level
     */
    void on_filterLevel_currentIndexChanged(int index);

    /**
     * @brief Private slot for changes of the sender filter
     *
     * @param text New text of the sender filter setting
     */
    void on_filterSender_currentTextChanged(const QString& text);

    /**
     * @brief Private slot for changes of the topic filter
     *
     * @param text New text of the topic filter setting
     */
    void on_filterTopic_currentTextChanged(const QString& text);

    /**
     * @brief Private slot for the "editing finished" signal from the message regex pattern
     */
    void on_filterMessage_editingFinished();

    /**
     * @brief Private slot for "Reset" button of the filter settings
     */
    void on_clearFilters_clicked();

    /**
     * @brief Private slot for "Clear messages" button
     */
    void on_clearMessages_clicked();

    /**
     * @brief Private slot for selecting items from the log view and displaying a dialog with details
     */
    void on_viewLog_activated(const QModelIndex& i);

private:
    /** Subscription pool listening to new log messages */
    QLogListener log_listener_;
    std::map<std::string, std::shared_ptr<QSenderSubscriptions>> senders_;

    /** Sorting and filtering proxy for displaying log messages */
    QLogFilter log_filter_;

    /** Item delegate for painting log message rows in the view */
    LogItemDelegate log_message_delegate_;

    /** Status bar for message count display */
    LogStatusBar status_bar_;

    /** Logger to use */
    constellation::log::Logger logger_;

    /** UI Settings */
    QSettings gui_settings_;
};
