/**
 * @file
 * @brief MissionControl GUI definition
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "MissionControl.hpp"

#include <chrono>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <argparse/argparse.hpp>
#include <magic_enum.hpp>
#include <zmq.hpp>

#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QException>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLinearGradient>
#include <QMenu>
#include <QMessageBox>
#include <QMetaType>
#include <QPainter>
#include <QRegularExpression>
#include <QSpinBox>
#include <QStyleOptionViewItem>
#include <QTextDocument>
#include <QtGlobal>
#include <QTimer>
#include <QTimeZone>

#include "constellation/build.hpp"
#include "constellation/controller/Controller.hpp"
#include "constellation/controller/ControllerConfiguration.hpp"
#include "constellation/controller/exceptions.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/log/SinkManager.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/string.hpp"

#include "qt_utils.hpp"

using namespace constellation;
using namespace constellation::chirp;
using namespace constellation::controller;
using namespace constellation::log;
using namespace constellation::protocol;
using namespace constellation::utils;

void ConnectionItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    auto options = option;
    initStyleOption(&options, index);

    painter->save();

    // Get sibling for column 7 (where the lives are stored) for current row:
    const auto lives = index.sibling(index.row(), 7).data().toInt();
    if(lives < 3 && index.column() >= 6) {
        const auto alpha = (3 - lives) * 85;
        QLinearGradient gradient(
            options.rect.left(), options.rect.center().y(), options.rect.right(), options.rect.center().y());
        gradient.setColorAt(0, QColor(255, 0, 0, (index.column() == 6 ? 0 : alpha)));
        gradient.setColorAt(1, QColor(255, 0, 0, alpha));
        painter->fillRect(options.rect, QBrush(gradient));
    }

    QTextDocument doc;
    doc.setHtml(options.text);

    options.text = "";
    options.widget->style()->drawControl(QStyle::CE_ItemViewItem, &options, painter);

    painter->translate(options.rect.left(), options.rect.top());
    const QRect clip(0, 0, options.rect.width(), options.rect.height());
    doc.drawContents(painter, clip);

    painter->restore();
}

QSize ConnectionItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QStyleOptionViewItem options = option;
    initStyleOption(&options, index);

    QTextDocument doc;
    doc.setHtml(options.text);
    doc.setTextWidth(options.rect.width());
    return {static_cast<int>(doc.idealWidth()), static_cast<int>(doc.size().height())};
}

FileSystemModel::FileSystemModel(QObject* parent) : QFileSystemModel(parent) {}

QVariant FileSystemModel::data(const QModelIndex& index, int role) const {
    if(role == Qt::DisplayRole && index.column() == 0) {
        return QDir::toNativeSeparators(filePath(index));
    }
    return QFileSystemModel::data(index, role);
}

MissionControl::MissionControl(std::string controller_name, std::string_view group_name)
    : runcontrol_(std::move(controller_name)), logger_("GUI"), user_logger_("OP"),
      run_id_validator_(QRegularExpression("^[\\w-]+$"), this), config_file_fs_(&config_file_completer_) {

    // Register types used in signals & slots:
    qRegisterMetaType<QModelIndex>("QModelIndex");
    qRegisterMetaType<constellation::protocol::CSCP::State>("constellation::protocol::CSCP::State");
    qRegisterMetaType<std::size_t>("std::size_t");

    // Set up the user interface
    setupUi(this);

    // Set initial values for header bar
    const auto state = runcontrol_.getLowestState();
    cnstlnName->setText(QString::fromStdString("<font color=gray><b>" + std::string(group_name) + "</b></font>"));
    labelState->setText(QController::getStyledState(state, runcontrol_.isInGlobalState()));
    labelNrSatellites->setText("<font color='gray'><b>" + QString::number(runcontrol_.getConnections().size()) +
                               "</b></font>");

    sorting_proxy_.setSourceModel(&runcontrol_);
    viewConn->setModel(&sorting_proxy_);
    viewConn->setItemDelegate(&item_delegate_);
    viewConn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(viewConn, &QTreeView::customContextMenuRequested, this, &MissionControl::custom_context_menu);

    // Set default column width of main connection view
    viewConn->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    viewConn->header()->resizeSection(0, 100);
    viewConn->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    viewConn->header()->resizeSection(1, 100);
    viewConn->header()->setSectionResizeMode(2, QHeaderView::Fixed);
    viewConn->header()->resizeSection(2, 120);
    viewConn->header()->setSectionResizeMode(3, QHeaderView::Interactive);
    viewConn->header()->resizeSection(3, 180);
    viewConn->header()->setSectionResizeMode(4, QHeaderView::Fixed);
    viewConn->header()->resizeSection(4, 140);
    viewConn->header()->setSectionResizeMode(5, QHeaderView::Stretch);
    viewConn->header()->setSectionResizeMode(6, QHeaderView::Interactive);
    viewConn->header()->resizeSection(6, 80);
    viewConn->header()->setSectionResizeMode(7, QHeaderView::Fixed);
    viewConn->header()->resizeSection(7, 40);

    const auto cfg_file = gui_settings_.value("run/configfile", "").toString();
    if(QFile::exists(cfg_file)) {
        txtConfigFileName->setText(cfg_file);
    }

    // Restore window geometry:
    restoreGeometry(gui_settings_.value("window/geometry", saveGeometry()).toByteArray());
    restoreState(gui_settings_.value("window/savestate", saveState()).toByteArray());
    move(gui_settings_.value("window/pos", pos()).toPoint());
    resize(gui_settings_.value("window/size", size()).toSize());
    if(gui_settings_.value("window/maximized", isMaximized()).toBool()) {
        showMaximized();
    }

    // Restore last run identifier from configuration:
    update_run_identifier(gui_settings_.value("run/identifier", "run").toString(),
                          gui_settings_.value("run/sequence", 0).toInt());

    setWindowTitle("Constellation MissionControl " CNSTLN_VERSION_FULL);

    // Connect timer to method for run timer update
    connect(&display_timer_, &QTimer::timeout, this, &MissionControl::update_run_infos);
    display_timer_.start(300); // internal update time of GUI

    // Connect run identifier edit boxes:
    connect(runIdentifier, &QLineEdit::editingFinished, this, [&]() {
        update_run_identifier(runIdentifier->text(), runSequence->value());
    });
    connect(runSequence, QOverload<int>::of(&QSpinBox::valueChanged), this, [&](int i) {
        update_run_identifier(runIdentifier->text(), i);
    });

    // Connect connection update signal:
    connect(&runcontrol_, &QController::connectionsChanged, this, [&](std::size_t num) {
        labelNrSatellites->setText("<font color='gray'><b>" + QString::number(num) + "</b></font>");
    });

    connect(&runcontrol_, &QController::connectionsChanged, this, &MissionControl::startup);

    // Connect state update signal:
    connect(&runcontrol_, &QController::reachedState, this, [&](CSCP::State state, bool global) {
        update_button_states(state);
        labelState->setText(QController::getStyledState(state, global));
    });

    // Attach validators & completers:
    runIdentifier->setValidator(&run_id_validator_);
    config_file_fs_.setRootPath({});
    config_file_completer_.setMaxVisibleItems(10);
    config_file_completer_.setModel(&config_file_fs_);
    config_file_completer_.setCompletionMode(QCompleter::InlineCompletion);
    txtConfigFileName->setCompleter(&config_file_completer_);

    // Start the controller
    runcontrol_.start();
}

void MissionControl::startup(std::size_t num) {

    // For the very first connection, try to obtain run time and run identifier
    if(num == 1) {
        const auto is_running = runcontrol_.isInState(CSCP::State::RUN);

        if(is_running) {
            auto run_time = runcontrol_.getRunStartTime();
            if(run_time.has_value()) {
                LOG(logger_, DEBUG) << "Fetched time from satellites, setting run timer to " << run_time.value();
                run_start_time_ = from_timepoint(run_time.value());
            }
        }

        // Read last run identifier from the connection:
        auto run_id = std::string(runcontrol_.getRunIdentifier());
        if(!run_id.empty()) {
            // attempt to find a sequence number:
            const std::size_t pos = run_id.find_last_of('_');
            auto identifier = (pos != std::string::npos ? run_id.substr(0, pos) : run_id);
            std::size_t sequence = 0;
            try {
                sequence = (pos != std::string::npos ? std::stoi(run_id.substr(pos + 1)) : 0);
            } catch(const std::invalid_argument&) {
                LOG(logger_, DEBUG) << "Could not detect a sequence number in run identifier, appending 0 instead";
            }

            // This is an old run identifier, increment the sequence:
            if(!is_running) {
                sequence++;
            }
            update_run_identifier(QString::fromStdString(identifier), static_cast<int>(sequence));
        }
    }
}

void MissionControl::update_run_identifier(const QString& text, int number) {

    runIdentifier->setText(text);
    runSequence->setValue(number);

    if(!text.isEmpty()) {
        current_run_ = text + "_";
    } else {
        current_run_.clear();
    }
    current_run_ += QString::number(number);

    gui_settings_.setValue("run/identifier", text);
    gui_settings_.setValue("run/sequence", number);

    LOG(logger_, DEBUG) << "Updated run identifier to " << current_run_.toStdString();
}

void MissionControl::on_btnInit_clicked() {
    // Read config file from UI
    auto configs = parse_config_file(txtConfigFileName->text());

    if(!configs.has_value()) {
        return;
    }

    for(auto& response : runcontrol_.sendQCommands("initialize", configs.value())) {
        LOG(logger_, DEBUG) << "Initialize: " << response.first << ": " << to_string(response.second.getVerb().first);
    }
}

void MissionControl::on_txtConfigFileName_textChanged() {
    // Updated config file name, trigger button update:
    const auto state = runcontrol_.getLowestState();
    update_button_states(state);
}

void MissionControl::on_btnShutdown_clicked() {
    // We don't close the GUI but shutdown satellites instead:
    if(QMessageBox::question(this, "Quitting", "Shutdown all satellites?", QMessageBox::Ok | QMessageBox::Cancel) ==
       QMessageBox::Cancel) {
        LOG(logger_, DEBUG) << "Aborted satellite shutdown";
    } else {
        for(auto& response : runcontrol_.sendQCommands("shutdown")) {
            LOG(logger_, DEBUG) << "Shutdown: " << response.first << ": " << to_string(response.second.getVerb().first);
        }
    }
}

void MissionControl::on_btnConfig_clicked() {
    for(auto& response : runcontrol_.sendQCommands("launch")) {
        LOG(logger_, DEBUG) << "Launch: " << response.first << ": " << to_string(response.second.getVerb().first);
    }
}

void MissionControl::on_btnLand_clicked() {
    for(auto& response : runcontrol_.sendQCommands("land")) {
        LOG(logger_, DEBUG) << "Land: " << response.first << ": " << to_string(response.second.getVerb().first);
    }
}

void MissionControl::on_btnStart_clicked() {
    for(auto& response : runcontrol_.sendQCommands("start", current_run_.toStdString())) {
        LOG(logger_, DEBUG) << "Start: " << response.first << ": " << to_string(response.second.getVerb().first);
    }

    // Set start time for this run
    run_start_time_ = QDateTime::currentDateTimeUtc();
}

void MissionControl::on_btnStop_clicked() {
    for(auto& response : runcontrol_.sendQCommands("stop")) {
        LOG(logger_, DEBUG) << "Stop: " << response.first << ": " << to_string(response.second.getVerb().first);
    }

    // Increment run sequence:
    runSequence->setValue(runSequence->value() + 1);
}

void MissionControl::on_btnLog_clicked() {
    const auto msg = txtLogmsg->text().toStdString();
    const auto level = static_cast<Level>(comboBoxLogLevel->currentIndex());
    LOG(user_logger_, level) << msg;
    txtLogmsg->clear();
}

void MissionControl::on_btnLoadConf_clicked() {
    const QString usedpath = QFileInfo(txtConfigFileName->text()).path();
    const QString filename =
        QFileDialog::getOpenFileName(this, tr("Open File"), usedpath, tr("Configurations (*.conf *.toml *.ini)"));
    if(!filename.isNull()) {
        txtConfigFileName->setText(filename);
    }
}

void MissionControl::update_button_states(CSCP::State state) {

    const QRegularExpression rx_conf(R"(.+(\.conf$|\.ini$|\.toml$))");
    auto m = rx_conf.match(txtConfigFileName->text());

    using enum CSCP::State;
    btnInit->setEnabled(CSCP::is_one_of_states<NEW, INIT, SAFE, ERROR>(state) && m.hasMatch());
    btnLand->setEnabled(state == ORBIT);
    btnConfig->setEnabled(state == INIT);
    btnLoadConf->setEnabled(CSCP::is_one_of_states<NEW, initializing, INIT, SAFE, ERROR>(state));
    txtConfigFileName->setEnabled(CSCP::is_one_of_states<NEW, initializing, INIT, SAFE, ERROR>(state));
    btnStart->setEnabled(state == ORBIT);
    btnStop->setEnabled(state == RUN);
    btnShutdown->setEnabled(CSCP::is_shutdown_allowed(state));

    // Deactivate run identifier fields during run:
    runIdentifier->setEnabled(CSCP::is_not_one_of_states<RUN, starting, stopping, interrupting>(state));
    runSequence->setEnabled(CSCP::is_not_one_of_states<RUN, starting, stopping, interrupting>(state));
}

void MissionControl::update_run_infos() {

    // Update run timer:
    if(runcontrol_.getLowestState() == CSCP::State::RUN) {
        auto duration = duration_string(std::chrono::seconds(run_start_time_.secsTo(QDateTime::currentDateTime())));
        runDuration->setStyleSheet("QLabel { font-weight: bold; color: black; }");
        runDuration->setText(duration);
        runID->setText("<b>" + current_run_ + "</b>");
    } else {
        runDuration->setStyleSheet("QLabel { font-weight: normal; color: gray; }");
        runID->setText("<font color=gray><b>" + current_run_ + "</b> (next)</font>");
    }
}

void MissionControl::closeEvent(QCloseEvent* event) {

    // Stop the controller:
    runcontrol_.stop();

    // Store window geometry:
    gui_settings_.setValue("window/geometry", saveGeometry());
    gui_settings_.setValue("window/savestate", saveState());
    gui_settings_.setValue("window/maximized", isMaximized());
    if(!isMaximized()) {
        gui_settings_.setValue("window/pos", pos());
        gui_settings_.setValue("window/size", size());
    }

    gui_settings_.setValue("run/configfile", txtConfigFileName->text());

    // Terminate the application
    event->accept();
}

void MissionControl::custom_context_menu(const QPoint& point) {
    // Use the sorting proxy to obtain the correct model index of the source:
    const QModelIndex index = sorting_proxy_.mapToSource(viewConn->indexAt(point));
    if(!index.isValid()) {
        return;
    }

    auto contextMenu = QMenu(viewConn);

    auto* initialiseAction = new QAction("Initialize", this);
    connect(initialiseAction, &QAction::triggered, this, [this, index]() {
        auto config = parse_config_file(txtConfigFileName->text(), index);
        if(!config.has_value()) {
            return;
        }
        runcontrol_.sendQCommand(index, "initialize", config.value());
    });
    contextMenu.addAction(initialiseAction);

    auto* launchAction = new QAction("Launch", this);
    connect(launchAction, &QAction::triggered, this, [this, index]() { runcontrol_.sendQCommand(index, "launch"); });
    contextMenu.addAction(launchAction);

    auto* landAction = new QAction("Land", this);
    connect(landAction, &QAction::triggered, this, [this, index]() { runcontrol_.sendQCommand(index, "land"); });
    contextMenu.addAction(landAction);

    auto* startAction = new QAction("Start", this);
    connect(startAction, &QAction::triggered, this, [this, index]() {
        runcontrol_.sendQCommand(index, "start", current_run_.toStdString());
    });
    contextMenu.addAction(startAction);

    auto* stopAction = new QAction("Stop", this);
    connect(stopAction, &QAction::triggered, this, [this, index]() { runcontrol_.sendQCommand(index, "stop"); });
    contextMenu.addAction(stopAction);

    auto* terminateAction = new QAction("Shutdown", this);
    connect(terminateAction, &QAction::triggered, this, [this, index]() { runcontrol_.sendQCommand(index, "shutdown"); });
    contextMenu.addAction(terminateAction);

    // Draw separator
    contextMenu.addSeparator();

    // Add standard commands
    for(const auto command : magic_enum::enum_names<CSCP::StandardCommand>()) {
        if(command == "shutdown") {
            // Already added above
            continue;
        }
        const auto command_str = to_string(command);
        auto* action = new QAction(QString::fromStdString(command_str), this);
        connect(action, &QAction::triggered, this, [this, index, command_str]() {
            auto response = runcontrol_.sendQCommand(index, command_str);
            if(response.has_value()) {
                QMessageBox::information(nullptr, "Satellite Response", QString::fromStdString(response.value()));
            }
        });
        contextMenu.addAction(action);
    }

    // Draw separator
    contextMenu.addSeparator();

    // Request possible commands from remote:
    auto dict = runcontrol_.getQCommands(index);
    for(const auto& [key, value] : dict) {
        // Filter out transition and standard commands to not list them twice
        if(magic_enum::enum_cast<CSCP::TransitionCommand>(key, magic_enum::case_insensitive).has_value() ||
           magic_enum::enum_cast<CSCP::StandardCommand>(key, magic_enum::case_insensitive).has_value()) {
            continue;
        }

        auto* action = new QAction(QString::fromStdString(key), this);
        connect(action, &QAction::triggered, this, [this, index, key]() {
            auto response = runcontrol_.sendQCommand(index, key);
            if(response.has_value()) {
                QMessageBox::information(nullptr, "Satellite Response", QString::fromStdString(response.value()));
            }
        });
        contextMenu.addAction(action);
    }

    contextMenu.exec(viewConn->viewport()->mapToGlobal(point));
}

std::optional<std::map<std::string, Controller::CommandPayload>> MissionControl::parse_config_file(const QString& file) {
    try {
        const auto configs = ControllerConfiguration(std::filesystem::path(file.toStdString()));
        // Convert to CommandPayloads:
        std::map<std::string, Controller::CommandPayload> payloads {};
        std::vector<std::string> sats_without_config {};
        for(const auto& satellite : runcontrol_.getConnections()) {
            if(!configs.hasSatelliteConfiguration(satellite)) {
                sats_without_config.push_back(satellite);
            }
            payloads.emplace(satellite, configs.getSatelliteConfiguration(satellite));
        }

        if(!sats_without_config.empty() &&
           QMessageBox::question(this,
                                 "Warning",
                                 "The following satellites do not have explicit configuration sections in the selected "
                                 "configuration file:\n" +
                                     QString::fromStdString(range_to_string(sats_without_config, "\n")) +
                                     "\n\nContinue anyway?",
                                 QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel) {
            return {};
        }
        return payloads;
    } catch(const ControllerError& error) {
        QMessageBox::warning(nullptr, "ERROR", QString::fromStdString(std::string("Parsing failed: ") + error.what()));
        return {};
    }
}

std::optional<Controller::CommandPayload> MissionControl::parse_config_file(const QString& file, const QModelIndex& index) {
    const auto name = runcontrol_.getQName(index);
    try {
        const auto configs = ControllerConfiguration(std::filesystem::path(file.toStdString()));

        if(!configs.hasSatelliteConfiguration(name) &&
           QMessageBox::question(
               this,
               "Warning",
               QString::fromStdString(name) +
                   " has no explicit configuration section in the selected configuration file\n\nContinue anyway?",
               QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel) {
            return {};
        }
        return configs.getSatelliteConfiguration(name);
    } catch(ControllerError& error) {
        QMessageBox::warning(nullptr, "ERROR", QString::fromStdString(std::string("Parsing failed: ") + error.what()));
    }
    return {};
}

namespace {
    // NOLINTNEXTLINE(*-avoid-c-arrays)
    void parse_args(int argc, char* argv[], argparse::ArgumentParser& parser) {
        // Controller name (-n)
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
} // namespace

int main(int argc, char** argv) {
    try {
        auto qapp = std::make_shared<QApplication>(argc, argv);

        try {
            QCoreApplication::setOrganizationName("Constellation");
            QCoreApplication::setOrganizationDomain("constellation.pages.desy.de");
            QCoreApplication::setApplicationName("MissionControl");
        } catch(const QException&) {
            std::cerr << "Failed to set up UI application\n" << std::flush;
            return 1;
        }

        // Ensure that ZeroMQ doesn't fail creating the CMDP sink
        try {
            SinkManager::getInstance();
        } catch(const zmq::error_t& error) {
            std::cerr << "Failed to initialize logging: " << error.what() << "\n" << std::flush;
            return 1;
        }

        // Get the default logger
        auto& logger = Logger::getDefault();

        // CLI parsing
        argparse::ArgumentParser parser {"MissionControl", CNSTLN_VERSION_FULL};
        try {
            parse_args(argc, argv, parser);
        } catch(const std::exception& error) {
            LOG(logger, CRITICAL) << "Argument parsing failed: " << error.what();
            LOG(logger, CRITICAL) << "Run " << std::quoted("MissionControl --help") << " for help";
            return 1;
        }

        // Set log level
        const auto default_level = magic_enum::enum_cast<Level>(get_arg(parser, "level"), magic_enum::case_insensitive);
        if(!default_level.has_value()) {
            LOG(logger, CRITICAL) << "Log level " << std::quoted(get_arg(parser, "level"))
                                  << " is not valid, possible values are: " << list_enum_names<Level>();
            return 1;
        }
        SinkManager::getInstance().setConsoleLevels(default_level.value());

        // Check broadcast and any address
        asio::ip::address_v4 brd_addr {};
        try {
            brd_addr = asio::ip::make_address_v4(get_arg(parser, "brd"));
        } catch(const asio::system_error& error) {
            LOG(logger, CRITICAL) << "Invalid broadcast address " << std::quoted(get_arg(parser, "brd"));
            return 1;
        }
        asio::ip::address_v4 any_addr {};
        try {
            any_addr = asio::ip::make_address_v4(get_arg(parser, "any"));
        } catch(const asio::system_error& error) {
            LOG(logger, CRITICAL) << "Invalid any address " << std::quoted(get_arg(parser, "any"));
            return 1;
        }

        // Check satellite name
        const auto controller_name = get_arg(parser, "name");

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
            chirp_manager = std::make_unique<chirp::Manager>(brd_addr, any_addr, group_name, controller_name);
            chirp_manager->setAsDefaultInstance();
            chirp_manager->start();
        } catch(const std::exception& error) {
            LOG(logger, CRITICAL) << "Failed to initiate network discovery: " << error.what();
        }

        // Register CMDP in CHIRP and set sender name for CMDP
        SinkManager::getInstance().enableCMDPSending(controller_name);

        try {
            MissionControl gui(controller_name, group_name);
            gui.show();
            return QCoreApplication::exec();
        } catch(const QException&) {
            std::cerr << "Failed to start UI application\n" << std::flush;
        }
    } catch(...) {
        std::cerr << "Failed to start UI\n" << std::flush;
    }
    return 1;
}
