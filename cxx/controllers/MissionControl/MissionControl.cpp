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
#include <format>
#include <fstream>
#include <iostream>
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QException>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QPainter>
#include <QSpinBox>
#include <QTextDocument>
#include <QTimeZone>
#include <string>

#include <argparse/argparse.hpp>

#include "constellation/controller/ControllerConfiguration.hpp"
#include "constellation/controller/exceptions.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/log/SinkManager.hpp"
#include "constellation/core/utils/casts.hpp"

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

MissionControl::MissionControl(std::string controller_name, std::string_view group_name)
    : runcontrol_(std::move(controller_name)), logger_("GUI"), user_logger_("OP") {

    qRegisterMetaType<QModelIndex>("QModelIndex");
    qRegisterMetaType<constellation::protocol::CSCP::State>("constellation::protocol::CSCP::State");
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

    // Pick up latest run identifier information - either from running Constellation or from settings
    auto run_id = std::string(runcontrol_.getRunIdentifier());
    if(run_id.empty()) {
        update_run_identifier(gui_settings_.value("run/identifier", "run").toString(),
                              gui_settings_.value("run/sequence", 0).toInt());
    } else {
        // Attempt to find sequence:
        const std::size_t pos = run_id.find_last_of('_');
        auto identifier = (pos != std::string::npos ? run_id.substr(0, pos) : run_id);
        std::size_t sequence = 0;
        try {
            sequence = (pos != std::string::npos ? std::stoi(run_id.substr(pos + 1)) : 0);
        } catch(std::invalid_argument&) {
        }

        // This is an old run identifier, increment the sequence:
        if(!runcontrol_.isInState(CSCP::State::RUN)) {
            sequence++;
        }
        update_run_identifier(QString::fromStdString(identifier), static_cast<int>(sequence));
    }

    // Pick up the current run timer from the constellation of available:
    auto run_time = runcontrol_.getRunStartTime();
    if(run_time.has_value()) {
        if(runcontrol_.isInState(CSCP::State::RUN)) {
            LOG(logger_, DEBUG) << "Fetched time from satellites, setting run timer to " << run_time.value();

            // FIXME somehow fromStdTimePoint is not found
            run_start_time_ =
                QDateTime(QDate(1970, 1, 1), QTime(0, 0, 0), QTimeZone::utc())
                    .addMSecs(
                        std::chrono::duration_cast<std::chrono::milliseconds>(run_time.value().time_since_epoch()).count());
        }
    }

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

    setWindowTitle("Constellation MissionControl " CNSTLN_VERSION);

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

    // Connect state update signal:
    connect(&runcontrol_, &QController::reachedState, this, [&](CSCP::State state, bool global) {
        update_button_states(state);
        labelState->setText(QController::getStyledState(state, global));
    });

    // Update button state once manually
    update_button_states(state);
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

    // Nothing read - nothing to do
    if(configs.empty()) {
        return;
    }

    for(auto& response : runcontrol_.sendCommands("initialize", configs)) {
        LOG(logger_, DEBUG) << "Initialize: " << response.first << ": " << utils::to_string(response.second.getVerb().first);
    }
}

void MissionControl::on_btnShutdown_clicked() {
    // We don't close the GUI but shutdown satellites instead:
    if(QMessageBox::question(this, "Quitting", "Shutdown all satellites?", QMessageBox::Ok | QMessageBox::Cancel) ==
       QMessageBox::Cancel) {
        LOG(logger_, DEBUG) << "Aborted satellite shutdown";
    } else {
        for(auto& response : runcontrol_.sendCommands("shutdown")) {
            LOG(logger_, DEBUG) << "Shutdown: " << response.first << ": "
                                << utils::to_string(response.second.getVerb().first);
        }
    }
}

void MissionControl::on_btnConfig_clicked() {
    for(auto& response : runcontrol_.sendCommands("launch")) {
        LOG(logger_, DEBUG) << "Launch: " << response.first << ": " << utils::to_string(response.second.getVerb().first);
    }
}

void MissionControl::on_btnLand_clicked() {
    for(auto& response : runcontrol_.sendCommands("land")) {
        LOG(logger_, DEBUG) << "Land: " << response.first << ": " << utils::to_string(response.second.getVerb().first);
    }
}

void MissionControl::on_btnStart_clicked() {
    for(auto& response : runcontrol_.sendCommands("start", current_run_.toStdString())) {
        LOG(logger_, DEBUG) << "Start: " << response.first << ": " << utils::to_string(response.second.getVerb().first);
    }

    // Set start time for this run
    run_start_time_ = QDateTime::currentDateTimeUtc();
}

void MissionControl::on_btnStop_clicked() {
    for(auto& response : runcontrol_.sendCommands("stop")) {
        LOG(logger_, DEBUG) << "Stop: " << response.first << ": " << utils::to_string(response.second.getVerb().first);
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

    btnInit->setEnabled((state == CSCP::State::NEW || state == CSCP::State::INIT || state == CSCP::State::ERROR ||
                         state == CSCP::State::SAFE) &&
                        m.hasMatch());

    btnLand->setEnabled(state == CSCP::State::ORBIT);
    btnConfig->setEnabled(state == CSCP::State::INIT);
    btnLoadConf->setEnabled(state != CSCP::State::RUN && state != CSCP::State::ORBIT);
    btnStart->setEnabled(state == CSCP::State::ORBIT);
    btnStop->setEnabled(state == CSCP::State::RUN);
    btnShutdown->setEnabled(state == CSCP::State::SAFE || state == CSCP::State::INIT || state == CSCP::State::NEW);

    // Deactivate run identifier fields during run:
    runIdentifier->setEnabled(state != CSCP::State::RUN && state != CSCP::State::starting && state != CSCP::State::stopping);
    runSequence->setEnabled(state != CSCP::State::RUN && state != CSCP::State::starting && state != CSCP::State::stopping);
}

void MissionControl::update_run_infos() {

    // Update run timer:
    if(runcontrol_.getLowestState() == CSCP::State::RUN) {
        auto duration =
            std::format("{:%H:%M:%S}", std::chrono::seconds(run_start_time_.secsTo(QDateTime::currentDateTime())));
        runDuration->setText("<b>" + QString::fromStdString(duration) + "</b>");
        runID->setText("<b>" + current_run_ + "</b>");
    } else {
        runDuration->setText("<font color=gray>" + runDuration->text() + "</font>");
        runID->setText("<font color=gray><b>" + current_run_ + "</b> (next)</font>");
    }
}

void MissionControl::closeEvent(QCloseEvent* event) {

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
    const QModelIndex index = viewConn->indexAt(point);
    if(!index.isValid()) {
        return;
    }

    auto contextMenu = QMenu(viewConn);

    auto* initialiseAction = new QAction("Initialize", this);
    connect(initialiseAction, &QAction::triggered, this, [this, index]() {
        auto config = parse_config_file(txtConfigFileName->text(), index);
        runcontrol_.sendQCommand(index, "initialize", config);
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

    // Request possible commands from remote:
    auto dict = runcontrol_.getQCommands(index);
    for(const auto& [key, value] : dict) {
        // Filter out transition commands to not list them twice
        if(magic_enum::enum_cast<CSCP::TransitionCommand>(key, magic_enum::case_insensitive).has_value()) {
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

std::map<std::string, Controller::CommandPayload> MissionControl::parse_config_file(const QString& file) {
    try {
        const auto configs = ControllerConfiguration(std::filesystem::path(file.toStdString()));
        // Convert to CommandPayloads:
        std::map<std::string, Controller::CommandPayload> payloads;
        for(const auto& satellite : runcontrol_.getConnections()) {
            payloads.emplace(satellite, configs.getSatelliteConfiguration(satellite));
        }
        return payloads;
    } catch(ControllerError& err) {
        QMessageBox::warning(nullptr, "ERROR", QString::fromStdString(std::string("Parsing failed: ") + err.what()));
        return {};
    }
}

Controller::CommandPayload MissionControl::parse_config_file(const QString& file, const QModelIndex& index) {
    const auto name = runcontrol_.getQName(index);
    try {
        const auto configs = ControllerConfiguration(std::filesystem::path(file.toStdString()));
        return configs.getSatelliteConfiguration(name);
    } catch(ControllerError& err) {
        QMessageBox::warning(nullptr, "ERROR", QString::fromStdString(std::string("Parsing failed: ") + err.what()));
    }
    return {};
}

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

int main(int argc, char** argv) {
    QCoreApplication* qapp = new QApplication(argc, argv);

    try {
        QCoreApplication::setOrganizationName("Constellation");
        QCoreApplication::setOrganizationDomain("constellation.pages.desy.de");
        QCoreApplication::setApplicationName("MissionControl");
    } catch(QException& e) {
        std::cerr << "Failed to set up UI application" << std::endl;
        return 1;
    }

    // Ensure that ZeroMQ doesn't fail creating the CMDP sink
    try {
        SinkManager::getInstance();
    } catch(const ZMQInitError& error) {
        std::cerr << "Failed to initialize logging: " << error.what() << std::endl;
        return 1;
    }

    // Get the default logger
    auto& logger = Logger::getDefault();

    // CLI parsing
    argparse::ArgumentParser parser {"MissionControl", CNSTLN_VERSION};
    try {
        parse_args(argc, argv, parser);
    } catch(const std::exception& error) {
        LOG(logger, CRITICAL) << "Argument parsing failed: " << error.what();
        LOG(logger, CRITICAL) << "Run \""
                              << "MissionControl"
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
    const auto controller_name = get_arg(parser, "name");

    // Log the version after all the basic checks are done
    LOG(logger, STATUS) << "Constellation v" << CNSTLN_VERSION;

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
    } catch(QException& e) {
        std::cerr << "Failed to start UI application" << std::endl;
        return 1;
    }
}
