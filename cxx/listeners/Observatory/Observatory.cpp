/**
 * @file
 * @brief Observatory GUI
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Observatory.hpp"

#include <iostream>
#include <QApplication>
#include <QInputDialog>

#include <argparse/argparse.hpp>
#include <magic_enum.hpp>

#include "constellation/core/log/log.hpp"
#include "constellation/core/log/SinkManager.hpp"
#include "constellation/core/utils/casts.hpp"

using namespace constellation;
using namespace constellation::chirp;
using namespace constellation::log;
using namespace constellation::utils;

LogItemDelegate::LogItemDelegate(QLogListener* model) : log_listener_(model) {}

void LogItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    const auto color = level_colors.at(log_listener_->getMessageLevel(index));
    painter->fillRect(option.rect, QBrush(color));
    QItemDelegate::paint(painter, option, index);
}

Observatory::Observatory(std::string_view group_name) : QMainWindow(), log_message_delegate_(&log_listener_) {

    qRegisterMetaType<QModelIndex>("QModelIndex");
    setupUi(this);

    // Connect signals:
    connect(&log_listener_, &QLogListener::newMessage, this, &Observatory::new_message_display);
    connect(&log_listener_, &QLogListener::newSender, this, [&](QString sender) {
        filterSender->addItem(sender);
        log_filter_.addSender(sender);
    });
    connect(&log_listener_, &QLogListener::newTopic, this, [&](QString topic) {
        filterTopic->addItem(topic);
        log_filter_.addTopic(topic);
    });

    // Start the log receiver pool
    log_listener_.startPool();

    // Set up header bar:
    cnstlnName->setText(QString::fromStdString("<font color=gray><b>" + std::string(group_name) + "</b></font>"));

    log_filter_.setSourceModel(&log_listener_);
    viewLog->setModel(&log_filter_);
    viewLog->setItemDelegate(&log_message_delegate_);
    for(int col = 0; col < LogMessage::countColumns(); ++col) {
        const int width = LogMessage::columnWidth(col);
        if(width >= 0)
            viewLog->setColumnWidth(col, width);
    }

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
        const auto level = magic_enum::enum_cast<Level>(qlevel.toStdString(), magic_enum::case_insensitive);
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
    const auto slevel = magic_enum::enum_cast<Level>(qslevel.toStdString(), magic_enum::case_insensitive);
    log_listener_.setGlobalSubscriptionLevel(slevel.value_or(Level::WARNING));
    globalLevel->setCurrentIndex(std::to_underlying(slevel.value_or(Level::WARNING)));
}

Observatory::~Observatory() {
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
    gui_settings_.setValue("subscriptions/level",
                           QString::fromStdString(to_string(log_listener_.getGlobalSubscriptionLevel())));
}

void Observatory::closeEvent(QCloseEvent*) {
    QApplication::quit();
}

void Observatory::on_filterLevel_currentIndexChanged(int index) {
    log_filter_.setFilterLevel(Level(index));
}

void Observatory::on_globalLevel_currentIndexChanged(int index) {
    log_listener_.subscribeToTopic(Level(index));
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
    new LogDialog(log_listener_.getMessage(i));
}

void Observatory::new_message_display(const QModelIndex&) {
    // FIXME it would be nice to only scroll when at the end and keep position otherwise
    viewLog->scrollToBottom();
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

// NOLINTNEXTLINE(*-avoid-c-arrays)
void parse_args(int argc, char* argv[], argparse::ArgumentParser& parser) {
    // Listener name (-n)
    parser.add_argument("-n", "--name").help("controller name").default_value("MissionControl");

    // Constellation group (-g)
    parser.add_argument("-g", "--group").help("group name");

    // Console log level (-l)
    parser.add_argument("-l", "--level").help("log level").default_value("INFO");

    // Broadcast address (--brd)
    std::string default_brd_addr {};
    try {
        default_brd_addr = asio::ip::address_v4::broadcast().to_string();
    } catch(const asio::system_error& error) {
        default_brd_addr = "255.255.255.255";
    }
    parser.add_argument("--brd").help("broadcast address").default_value(default_brd_addr);

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

int main(int argc, char** argv) {
    QCoreApplication* qapp = new QApplication(argc, argv);

    QCoreApplication::setOrganizationName("Constellation");
    QCoreApplication::setOrganizationDomain("constellation.pages.desy.de");
    QCoreApplication::setApplicationName("Observatory");

    // Get the default logger
    auto& logger = Logger::getDefault();

    // CLI parsing
    argparse::ArgumentParser parser {"Observatory", CNSTLN_VERSION};
    try {
        parse_args(argc, argv, parser);
    } catch(const std::exception& error) {
        LOG(logger, CRITICAL) << "Argument parsing failed: " << error.what();
        LOG(logger, CRITICAL) << "Run \""
                              << "Observatory"
                              << " --help\" for help";
        return 1;
    }

    // Set log level
    const auto default_level = magic_enum::enum_cast<Level>(get_arg(parser, "level"), magic_enum::case_insensitive);
    if(!default_level.has_value()) {
        LOG(logger, CRITICAL) << "Log level \"" << get_arg(parser, "level") << "\" is not valid"
                              << ", possible values are: " << utils::list_enum_names<Level>();
        return 1;
    }
    SinkManager::getInstance().setConsoleLevels(default_level.value());

    // Check broadcast and any address
    asio::ip::address_v4 brd_addr {};
    try {
        brd_addr = asio::ip::make_address_v4(get_arg(parser, "brd"));
    } catch(const asio::system_error& error) {
        LOG(logger, CRITICAL) << "Invalid broadcast address \"" << get_arg(parser, "brd") << "\"";
        return 1;
    }
    asio::ip::address_v4 any_addr {};
    try {
        any_addr = asio::ip::make_address_v4(get_arg(parser, "any"));
    } catch(const asio::system_error& error) {
        LOG(logger, CRITICAL) << "Invalid any address \"" << get_arg(parser, "any") << "\"";
        return 1;
    }

    // Check satellite name
    const auto logger_name = get_arg(parser, "name");

    // Log the version after all the basic checks are done
    LOG(logger, STATUS) << "Constellation v" << CNSTLN_VERSION;

    // Get Constellation group:
    std::string group_name;
    if(parser.is_used("group")) {
        group_name = get_arg(parser, "group");
    } else {
        QString text = QInputDialog::getText(NULL, "Constellation", "Constellation group to connect to:", QLineEdit::Normal);
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

    Observatory gui(group_name);
    gui.show();

    return qapp->exec();
}
