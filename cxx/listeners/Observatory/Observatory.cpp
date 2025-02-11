/**
 * @file
 * @brief Observatory GUI
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Observatory.hpp"

#include <cstddef>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <argparse/argparse.hpp>

#include <QApplication>
#include <QBrush>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QException>
#include <QInputDialog>
#include <QMetaType>
#include <QModelIndex>
#include <QPainter>
#include <QPalette>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTreeWidgetItem>

#include "constellation/build.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/log/SinkManager.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/gui/QLogMessage.hpp"
#include "constellation/gui/QLogMessageDialog.hpp"
#include "constellation/gui/qt_utils.hpp"

#include "QLogListener.hpp"

using namespace constellation;
using namespace constellation::chirp;
using namespace constellation::gui;
using namespace constellation::log;
using namespace constellation::utils;

LogStatusBar::LogStatusBar()
    : layout_(this), label_all_(new QLabel("0 messages")), label_critical_(new QLabel()), label_warning_(new QLabel()) {

    label_all_->setStyleSheet("QLabel { font-size: 12px; font-weight: normal; color: gray; }");
    label_critical_->setStyleSheet("QLabel { font-size: 12px; font-weight: bold; color: red; }");
    label_warning_->setStyleSheet("QLabel { font-size: 12px; font-weight: bold; color: orange; }");

    layout_.addWidget(label_critical_);
    layout_.addWidget(label_warning_);
    layout_.addWidget(label_all_);
}

void LogStatusBar::resetMessageCounts() {
    msg_all_ = 0;
    msg_warning_ = 0;
    msg_critical_ = 0;

    label_all_->setText(QString::number(msg_all_) + " messages");
    label_warning_->setText("");
    label_critical_->setText("");
}

void LogStatusBar::countMessage(Level level) {

    label_all_->setText(QString::number(++msg_all_) + " messages");

    if(level == Level::WARNING) {
        label_warning_->setText(QString::number(++msg_warning_) + " warnings");
    }

    if(level == Level::CRITICAL) {
        label_critical_->setText(QString::number(++msg_critical_) + " errors");
    }
}

void LogItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    auto options = option;

    // Get sibling for column 2 (where the log level is stored) for current row:
    const QModelIndex lvl_index = index.sibling(index.row(), 2);

    // Get log level color
    const auto level_str = lvl_index.data().toString().toStdString();
    const auto level = enum_cast<Level>(level_str).value_or(WARNING);

    const auto color = get_log_level_color(level);
    if(level > Level::INFO) {
        // High levels get background coloring
        painter->fillRect(options.rect, QBrush(color));
    } else {
        // Others just text color adjustment
        options.palette.setColor(QPalette::Text, color);
    }

    QStyledItemDelegate::paint(painter, options, index);
}

QString LogItemDelegate::displayText(const QVariant& value, const QLocale& locale) const {
    if(value.userType() == QMetaType::QDateTime) {
        return locale.toString(value.toDateTime(), "yyyy-MM-dd hh:mm:ss");
    }
    return QStyledItemDelegate::displayText(value, locale);
}

Observatory::Observatory(std::string_view group_name) : logger_("UI") {

    qRegisterMetaType<QModelIndex>("QModelIndex");
    setupUi(this);

    setWindowTitle("Constellation Observatory " CNSTLN_VERSION_FULL);

    // Connect signals:
    connect(&log_listener_, &QLogListener::newSender, this, [&](const QString& sender) { filterSender->addItem(sender); });
    connect(&log_listener_, &QLogListener::newTopic, this, [&](const QString& topic) { filterTopic->addItem(topic); });
    connect(&log_listener_, &QLogListener::newTopics, this, [&](const QStringList& topics) {
        filterTopic->clear();
        filterTopic->addItem("- All -");
        filterTopic->addItems(topics);
    });
    connect(&log_listener_, &QLogListener::connectionsChanged, this, [&](std::size_t num) {
        labelNrSatellites->setText("<font color='gray'><b>" + QString::number(num) + "</b></font>");
    });

    // Start the log receiver pool
    log_listener_.startPool();

    // Set up header bar:
    cnstlnName->setText(QString::fromStdString("<font color=gray><b>" + std::string(group_name) + "</b></font>"));

    log_filter_.setSourceModel(&log_listener_);
    viewLog->setModel(&log_filter_);
    viewLog->setItemDelegate(&log_message_delegate_);
    for(int col = 0; col < QLogMessage::countColumns(); ++col) {
        const int width = QLogMessage::columnWidth(col);
        if(width >= 0) {
            viewLog->setColumnWidth(col, width);
        }
    }
    // Enable uniform row height to allow for optimizations on Qt end:
    viewLog->setUniformRowHeights(true);

    // Restore window geometry:
    restoreGeometry(gui_settings_.value("window/geometry", saveGeometry()).toByteArray());
    restoreState(gui_settings_.value("window/savestate", saveState()).toByteArray());
    move(gui_settings_.value("window/pos", pos()).toPoint());
    resize(gui_settings_.value("window/size", size()).toSize());
    if(gui_settings_.value("window/maximized", isMaximized()).toBool()) {
        showMaximized();
    }

    // Load last filter settings:
    if(gui_settings_.contains("filters/level")) {
        const auto qlevel = gui_settings_.value("filters/level").toString();
        const auto level = enum_cast<Level>(qlevel.toStdString());
        log_filter_.setFilterLevel(level.value_or(Level::TRACE));
        filterLevel->setCurrentIndex(std::to_underlying(level.value_or(Level::TRACE)));
    }
    if(gui_settings_.contains("filters/sender")) {
        const auto sender = gui_settings_.value("filters/sender").toString();
        log_filter_.setFilterSender(sender.toStdString());
        filterSender->setCurrentText(QString::fromStdString(log_filter_.getFilterSender()));
    }
    if(gui_settings_.contains("filters/topic")) {
        const auto topic = gui_settings_.value("filters/topic").toString();
        log_filter_.setFilterTopic(topic.toStdString());
        filterTopic->setCurrentText(QString::fromStdString(log_filter_.getFilterTopic()));
    }
    const auto pattern = gui_settings_.value("filters/search", "");
    log_filter_.setFilterMessage(pattern.toString());
    filterMessage->setText(pattern.toString());

    // Load last subscription:
    const auto qslevel = gui_settings_.value("subscriptions/level").toString();
    const auto slevel = enum_cast<Level>(qslevel.toStdString());
    log_listener_.setGlobalLogLevel(slevel.value_or(Level::WARNING));
    globalLevel->setCurrentIndex(std::to_underlying(slevel.value_or(Level::WARNING)));

    // Set up status bar:
    statusBar()->addPermanentWidget(&status_bar_);
    connect(
        &log_listener_, &QLogListener::newMessage, this, [&](QModelIndex, Level level) { status_bar_.countMessage(level); });
    statusBar()->showMessage("Startup Complete", 2000);
}

void Observatory::closeEvent(QCloseEvent* event) {
    // Stop the log receiver
    log_listener_.stopPool();

    // Store window geometry:
    gui_settings_.setValue("window/geometry", saveGeometry());
    gui_settings_.setValue("window/savestate", saveState());
    gui_settings_.setValue("window/maximized", isMaximized());
    if(!isMaximized()) {
        gui_settings_.setValue("window/pos", pos());
        gui_settings_.setValue("window/size", size());
    }

    // Store filter settings
    gui_settings_.setValue("filters/level", QString::fromStdString(to_string(log_filter_.getFilterLevel())));
    gui_settings_.setValue("filters/sender", QString::fromStdString(log_filter_.getFilterSender()));
    gui_settings_.setValue("filters/topic", QString::fromStdString(log_filter_.getFilterTopic()));
    gui_settings_.setValue("filters/search", log_filter_.getFilterMessage());

    // Store subscription settings
    gui_settings_.setValue("subscriptions/level", QString::fromStdString(to_string(log_listener_.getGlobalLogLevel())));

    // Terminate the application
    event->accept();
}

void Observatory::on_filterLevel_currentIndexChanged(int index) {
    log_filter_.setFilterLevel(Level(index));
}

void Observatory::on_globalLevel_currentIndexChanged(int index) {
    log_listener_.setGlobalLogLevel(Level(index));
}

void Observatory::on_filterSender_currentTextChanged(const QString& text) {
    log_filter_.setFilterSender(text.toStdString());
}

void Observatory::on_filterTopic_currentTextChanged(const QString& text) {
    log_filter_.setFilterTopic(text.toStdString());
}

void Observatory::on_filterMessage_editingFinished() {
    log_filter_.setFilterMessage(filterMessage->displayText());
}

void Observatory::on_viewLog_activated(const QModelIndex& i) {
    // Translate to source index:
    const QModelIndex index = log_filter_.mapToSource(i);
    new QLogMessageDialog(this, log_listener_.getMessage(index));
}

void Observatory::on_clearFilters_clicked() {
    // Reset all filters
    filterLevel->setCurrentIndex(0);
    filterSender->setCurrentIndex(0);
    filterTopic->setCurrentIndex(0);

    // Setting the text does not emit the editingFinished signal, do it manually
    filterMessage->setText("");
    log_filter_.setFilterMessage("");
}

void Observatory::on_clearMessages_clicked() {
    log_listener_.clearMessages();
    status_bar_.resetMessageCounts();
}

namespace {
    // NOLINTNEXTLINE(*-avoid-c-arrays)
    void parse_args(int argc, char* argv[], argparse::ArgumentParser& parser) {
        // Listener name (-n)
        parser.add_argument("-n", "--name").help("listener name").default_value("Observatory");

        // Constellation group (-g)
        parser.add_argument("-g", "--group").help("group name");

        // Console log level (-l)
        parser.add_argument("-l", "--level").help("log level").default_value("INFO");

        // Broadcast address (--brd)
        parser.add_argument("--brd").help("broadcast address");

        // Any address (--any)
        std::string default_any_addr {};
        try {
            default_any_addr = asio::ip::address_v4::any().to_string();
        } catch(const asio::system_error& error) {
            default_any_addr = "0.0.0.0";
        }
        parser.add_argument("--any").help("any address").default_value(default_any_addr);

        // Note: this might throw
        parser.parse_args(argc, argv);
    }

    // parser.get() might throw a logic error, but this never happens in practice
    std::string get_arg(argparse::ArgumentParser& parser, std::string_view arg) noexcept {
        try {
            return parser.get(arg);
        } catch(const std::exception&) {
            std::unreachable();
        }
    }
} // namespace

int main(int argc, char** argv) {
    try {
        auto qapp = std::make_shared<QApplication>(argc, argv);

        try {
            QCoreApplication::setOrganizationName("Constellation");
            QCoreApplication::setOrganizationDomain("constellation.pages.desy.de");
            QCoreApplication::setApplicationName("Observatory");
        } catch(const QException&) {
            std::cerr << "Failed to set up UI application\n" << std::flush;
            return 1;
        }

        // Get the default logger
        auto& logger = Logger::getDefault();

        // CLI parsing
        argparse::ArgumentParser parser {"Observatory", CNSTLN_VERSION_FULL};
        try {
            parse_args(argc, argv, parser);
        } catch(const std::exception& error) {
            LOG(logger, CRITICAL) << "Argument parsing failed: " << error.what();
            LOG(logger, CRITICAL) << "Run " << std::quoted("Observatory --help") << " for help";

            return 1;
        }

        // Set log level
        const auto default_level = enum_cast<Level>(get_arg(parser, "level"));
        if(!default_level.has_value()) {
            LOG(logger, CRITICAL) << "Log level " << std::quoted(get_arg(parser, "level"))
                                  << " is not valid, possible values are: " << list_enum_names<Level>();
            return 1;
        }
        SinkManager::getInstance().setConsoleLevels(default_level.value());

        // Check broadcast and any address
        std::optional<asio::ip::address_v4> brd_addr {};
        try {
            const auto brd_string = parser.present("brd");
            if(brd_string.has_value()) {
                brd_addr = asio::ip::make_address_v4(brd_string.value());
            }
        } catch(const asio::system_error& error) {
            LOG(logger, CRITICAL) << "Invalid broadcast address \"" << get_arg(parser, "brd") << "\"";
            return 1;
        } catch(const std::exception&) {
            std::unreachable();
        }

        asio::ip::address_v4 any_addr {};
        try {
            any_addr = asio::ip::make_address_v4(get_arg(parser, "any"));
        } catch(const asio::system_error& error) {
            LOG(logger, CRITICAL) << "Invalid any address " << std::quoted(get_arg(parser, "any"));
            return 1;
        }

        // Check satellite name
        const auto logger_name = get_arg(parser, "name");

        // Log the version after all the basic checks are done
        LOG(logger, STATUS) << "Constellation " << CNSTLN_VERSION_FULL;

        // Get Constellation group:
        std::string group_name;
        if(parser.is_used("group")) {
            group_name = get_arg(parser, "group");
        } else {
            const QString text =
                QInputDialog::getText(nullptr, "Constellation", "Constellation group to connect to:", QLineEdit::Normal);
            if(!text.isEmpty()) {
                group_name = text.toStdString();
            } else {
                LOG(logger, CRITICAL) << "Invalid or empty constellation group name";
                return 1;
            }
        }

        // Create CHIRP manager and set as default
        std::unique_ptr<chirp::Manager> chirp_manager {};
        try {
            chirp_manager = std::make_unique<chirp::Manager>(brd_addr, any_addr, group_name, logger_name);
            chirp_manager->setAsDefaultInstance();
            chirp_manager->start();
        } catch(const std::exception& error) {
            LOG(logger, CRITICAL) << "Failed to initiate network discovery: " << error.what();
        }

        // Register CMDP in CHIRP and set sender name for CMDP
        SinkManager::getInstance().enableCMDPSending(logger_name);

        try {
            Observatory gui(group_name);
            gui.show();
            return QCoreApplication::exec();
        } catch(const QException&) {
            std::cerr << "Failed to start UI application\n" << std::flush;
        }
    } catch(...) {
        std::cerr << "Failed to start UI application\n";
        return 1;
    }
}
