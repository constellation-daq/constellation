/**
 * @file
 * @brief spdlog sink for the Qt QStatusBar
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <mutex>
#include <string_view>

#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/base_sink.h>

#include <QStatusBar>

/**
 * @class QStatusBarSink
 * @brief spdlog sink which displays the last-logged message as status bar message via QStatusBar->showMessage().
 * @brief This sink is meant as a convenience for displaying the last log message on the status bar of a Qt MainWindow. The
 * length of display before clearing can be configured, setting 0 will keep the message until replaced by the next.
 */
class QStatusBarSink : public spdlog::sinks::base_sink<std::mutex> {
public:
    /**
     * @brief Constructor of the sink
     *
     * @param qt_status_bar Pointer to the QStatusBar object to be used for logging
     * @param time Time to display each message in milliseconds, 0 keeps the message until replaced
     */
    QStatusBarSink(QStatusBar* qt_status_bar, int time) : qt_status_bar_(qt_status_bar), time_(time) {
        if(!qt_status_bar_) {
            throw std::invalid_argument("status_bar is null");
        }
    }

    ~QStatusBarSink() { flush_(); }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<std::mutex>::formatter_->format(msg, formatted);

        const auto str = std::string_view(formatted.data(), formatted.size());
        // apply the color to the color range in the formatted message.
        auto text = QString::fromLatin1(str.data(), static_cast<int>(str.size()));

        QMetaObject::invokeMethod(
            qt_status_bar_, [this, text]() { invoke_method_(text, qt_status_bar_, time_); }, Qt::AutoConnection);
    }

    void flush_() override {}

    /**
     * @brief Display the text on the status bar
     * @details This method is invoked in the GUI thread. It is a static method to ensure that it is handled correctly even
     * if the sink is destroyed prematurely before it is invoked.
     *
     * @param msg The message to be displayed
     * @param qt_status_bar Pointer to the status bar
     * @param time Time to display the message
     */
    static void invoke_method_(QString msg, QStatusBar* qt_status_bar, int time) { qt_status_bar->showMessage(msg, time); }

private:
    QStatusBar* qt_status_bar_;
    int time_;
};
