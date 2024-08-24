#include "MissionControl.hpp"

#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <QApplication>
#include <QDateTime>
#include <QSpinBox>
#include <QTimeZone>
#include <string>

#include <argparse/argparse.hpp>

#include "constellation/controller/ConfigParser.hpp"
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

MissionControl::MissionControl(std::string controller_name, std::string_view group_name)
    : QMainWindow(), runcontrol_(std::move(controller_name)), logger_("GUI"), user_logger_("OP") {

    qRegisterMetaType<QModelIndex>("QModelIndex");
    setupUi(this);

    // Set initial values for header bar
    cnstlnName->setText(QString::fromStdString("<font color=gray><b>" + std::string(group_name) + "</b></font>"));
    labelState->setText(get_state_str(runcontrol_.getLowestState(), runcontrol_.isInGlobalState()));
    labelNrSatellites->setText("<font color='gray'><b>" + QString::number(runcontrol_.getConnections().size()) +
                               "</b></font>");

    sorting_proxy_.setSourceModel(&runcontrol_);
    viewConn->setModel(&sorting_proxy_);
    viewConn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(viewConn, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onCustomContextMenu(const QPoint&)));

    // Pick up latest run identifier information - either from running Constellation or from settings
    auto run_id = std::string(runcontrol_.getRunIdentifier());
    if(run_id.empty()) {
        update_run_identifier(gui_settings_.value("run/identifier", "run").toString(),
                              gui_settings_.value("run/sequence", 0).toInt());
    } else {
        // Attempt to find sequence:
        std::size_t pos = run_id.find_last_of("_");
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
        update_run_identifier(QString::fromStdString(identifier), sequence);
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

    m_lastexit_success = gui_settings_.value("successexit", 1).toUInt();
    // TODO: check last if last file exits. if not, use default value.
    txtConfigFileName->setText(gui_settings_.value("run/configfile", "config file not set").toString());

    QRect geom(-1, -1, 150, 200);
    QRect geom_from_last_program_run;
    geom_from_last_program_run.setSize(gui_settings_.value("window/size", geom.size()).toSize());
    geom_from_last_program_run.moveTo(gui_settings_.value("window/pos", geom.topLeft()).toPoint());
    QSize fsize = frameGeometry().size();
    if((geom.x() == -1) || (geom.y() == -1) || (geom.width() == -1) || (geom.height() == -1)) {
        if((geom_from_last_program_run.x() == -1) || (geom_from_last_program_run.y() == -1) ||
           (geom_from_last_program_run.width() == -1) || (geom_from_last_program_run.height() == -1)) {
            geom.setX(x());
            geom.setY(y());
            geom.setWidth(fsize.width());
            geom.setHeight(fsize.height());
            move(geom.topLeft());
            resize(geom.size());
        } else {
            move(geom_from_last_program_run.topLeft());
            resize(geom_from_last_program_run.size());
        }
    }

    setWindowTitle("Constellation MissionControl " CNSTLN_VERSION);

    // Connect timer to gui update method
    connect(&m_timer_display, &QTimer::timeout, this, &MissionControl::updateInfos);
    m_timer_display.start(300); // internal update time of GUI

    // Connect run identifier edit boxes:
    connect(runIdentifier, &QLineEdit::editingFinished, this, [&]() {
        update_run_identifier(runIdentifier->text(), runSequence->value());
    });
    connect(runSequence, &QSpinBox::valueChanged, this, [&](int i) { update_run_identifier(runIdentifier->text(), i); });

    // Connect connection update signal:
    connect(&runcontrol_, &QController::connectionsChanged, this, [&](std::size_t num) {
        labelNrSatellites->setText("<font color='gray'><b>" + QString::number(num) + "</b></font>");
    });

    // Connect state update signal:
    connect(&runcontrol_, &QController::reachedGlobalState, this, [&](CSCP::State state) {
        labelState->setText(get_state_str(state, true));
    });
    connect(&runcontrol_, &QController::reachedLowestState, this, [&](CSCP::State state) {
        labelState->setText(get_state_str(state, false));
    });

    gui_settings_.setValue("successexit", 0);
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
    auto configs = parseConfigFile(txtConfigFileName->text());

    // Nothing read - nothing to do
    if(configs.empty()) {
        return;
    }

    auto responses = runcontrol_.sendCommands("initialize", configs);
    for(auto& response : responses) {
        LOG(logger_, DEBUG) << "Initialize: " << response.first << ": " << utils::to_string(response.second.getVerb().first);
    }
}

void MissionControl::on_btnShutdown_clicked() {
    // We don't close the GUI but shutdown satellites instead:
    if(QMessageBox::question(this, "Quitting", "Shutdown all satellites?", QMessageBox::Ok | QMessageBox::Cancel) ==
       QMessageBox::Cancel) {
        LOG(logger_, DEBUG) << "Aborted satellite shutdown";
    } else {
        auto responses = runcontrol_.sendCommands("shutdown");
        for(auto& response : responses) {
            LOG(logger_, DEBUG) << "Shutdown: " << response.first << ": "
                                << utils::to_string(response.second.getVerb().first);
        }
    }
}

void MissionControl::on_btnConfig_clicked() {
    auto responses = runcontrol_.sendCommands("launch");
    for(auto& response : responses) {
        LOG(logger_, DEBUG) << "Launch: " << response.first << ": " << utils::to_string(response.second.getVerb().first);
    }
}

void MissionControl::on_btnLand_clicked() {
    auto responses = runcontrol_.sendCommands("land");
    for(auto& response : responses) {
        LOG(logger_, DEBUG) << "Land: " << response.first << ": " << utils::to_string(response.second.getVerb().first);
    }
}

void MissionControl::on_btnStart_clicked() {
    auto responses = runcontrol_.sendCommands("start", current_run_.toStdString());

    // FIXME check that all started
    for(auto& response : responses) {
        LOG(logger_, DEBUG) << "Start: " << response.first << ": " << utils::to_string(response.second.getVerb().first);
    }

    // Set start time for this run
    run_start_time_ = QDateTime::currentDateTimeUtc();
}

void MissionControl::on_btnStop_clicked() {
    auto responses = runcontrol_.sendCommands("stop");
    for(auto& response : responses) {
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
    QString usedpath = QFileInfo(txtConfigFileName->text()).path();
    QString filename =
        QFileDialog::getOpenFileName(this, tr("Open File"), usedpath, tr("Configurations (*.conf *.toml *.ini)"));
    if(!filename.isNull()) {
        txtConfigFileName->setText(filename);
    }
}

void MissionControl::updateInfos() {

    // FIXME revisit what needs to be done here. Most infos are updated in the background by the controller via heartbeats!
    // We might need to handle metrics here and call addStatusDisoplay and removeStatusDisplay.

    auto state = runcontrol_.getLowestState();

    QRegularExpression rx_conf(".+(\\.conf$|\\.ini$|\\.toml$)");
    auto m = rx_conf.match(txtConfigFileName->text());

    btnInit->setEnabled((state == CSCP::State::NEW || state == CSCP::State::INIT || state == CSCP::State::ERROR ||
                         state == CSCP::State::SAFE) &&
                        m.hasMatch());

    btnLand->setEnabled(state == CSCP::State::ORBIT);
    btnConfig->setEnabled(state == CSCP::State::INIT);
    btnLoadConf->setEnabled(state != CSCP::State::RUN || state != CSCP::State::ORBIT);
    btnStart->setEnabled(state == CSCP::State::ORBIT);
    btnStop->setEnabled(state == CSCP::State::RUN);
    btnShutdown->setEnabled(state == CSCP::State::SAFE || state == CSCP::State::INIT || state == CSCP::State::NEW);

    // Deactivate run identifier fields during run:
    runIdentifier->setEnabled(state != CSCP::State::RUN && state != CSCP::State::starting && state != CSCP::State::stopping);
    runSequence->setEnabled(state != CSCP::State::RUN && state != CSCP::State::starting && state != CSCP::State::stopping);

    // Update run timer:
    if(state == CSCP::State::RUN) {
        auto duration =
            std::format("{:%H:%M:%S}", std::chrono::seconds(run_start_time_.secsTo(QDateTime::currentDateTime())));
        runDuration->setText("<b>" + QString::fromStdString(duration) + "</b>");
    } else {
        runDuration->setText("<font color=gray>" + runDuration->text() + "</font>");
    }

    // Update run identifier:
    if(state == CSCP::State::RUN) {
        runID->setText("<b>" + current_run_ + "</b>");
    } else {
        runID->setText("<font color=gray><b>" + current_run_ + "</b> (next)</font>");
    }
}

void MissionControl::closeEvent(QCloseEvent* event) {
    gui_settings_.setValue("window/size", size());
    gui_settings_.setValue("window/pos", pos());
    gui_settings_.setValue("run/configfile", txtConfigFileName->text());
    gui_settings_.setValue("successexit", 1);

    // Terminate the application
    event->accept();
}

void MissionControl::Exec() {
    show();
    if(QApplication::instance()) {
        QApplication::instance()->exec();
    } else {
        LOG(logger_, CRITICAL) << "Error executing the MissionControl GUI";
    }
}

QString MissionControl::get_state_str(CSCP::State state, bool global) const {

    QString global_indicatior = (global ? "" : " ≊");

    switch(state) {
    case CSCP::State::NEW: {
        return "<font color='gray'><b>New</b>" + global_indicatior + "</font>";
    }
    case CSCP::State::initializing: {
        return "<font color='gray'><b>Initializing...</b>" + global_indicatior + "</font>";
    }
    case CSCP::State::INIT: {
        return "<font color='gray'><b>Initialized</b>" + global_indicatior + "</font>";
    }
    case CSCP::State::launching: {
        return "<font color='orange'><b>Launching...</b>" + global_indicatior + "</font>";
    }
    case CSCP::State::landing: {
        return "<font color='orange'><b>Landing...</b>" + global_indicatior + "</font>";
    }
    case CSCP::State::reconfiguring: {
        return "<font color='orange'><b>Reconfiguring...</b>" + global_indicatior + "</font>";
    }
    case CSCP::State::ORBIT: {
        return "<font color='orange'><b>Orbiting</b>" + global_indicatior + "</font>";
    }
    case CSCP::State::starting: {
        return "<font color='green'><b>Starting...</b>" + global_indicatior + "</font>";
    }
    case CSCP::State::stopping: {
        return "<font color='green'><b>Stopping...</b>" + global_indicatior + "</font>";
    }
    case CSCP::State::RUN: {
        return "<font color='green'><b>Running</b>" + global_indicatior + "</font>";
    }
    case CSCP::State::SAFE: {
        return "<font color='red'><b>Safe Mode</b>" + global_indicatior + "</font>";
    }
    case CSCP::State::interrupting: {
        return "<font color='red'><b>Interrupting...</b>" + global_indicatior + "</font>";
    }
    case CSCP::State::ERROR: {
        return "<font color='darkred'><b>Error</b>" + global_indicatior + "</font>";
    }
    default: std::unreachable();
    }
}

void MissionControl::onCustomContextMenu(const QPoint& point) {
    QModelIndex index = viewConn->indexAt(point);
    if(!index.isValid()) {
        return;
    }

    QMenu* contextMenu = new QMenu(viewConn);

    QAction* initialiseAction = new QAction("Initialize", this);
    connect(initialiseAction, &QAction::triggered, this, [this, index]() {
        auto config = parseConfigFile(txtConfigFileName->text(), index);
        runcontrol_.sendQCommand(index, "initialize", config);
    });
    contextMenu->addAction(initialiseAction);

    QAction* launchAction = new QAction("Launch", this);
    connect(launchAction, &QAction::triggered, this, [this, index]() { runcontrol_.sendQCommand(index, "launch"); });
    contextMenu->addAction(launchAction);

    QAction* landAction = new QAction("Land", this);
    connect(landAction, &QAction::triggered, this, [this, index]() { runcontrol_.sendQCommand(index, "land"); });
    contextMenu->addAction(landAction);

    QAction* startAction = new QAction("Start", this);
    connect(startAction, &QAction::triggered, this, [this, index]() {
        runcontrol_.sendQCommand(index, "start", current_run_.toStdString());
    });
    contextMenu->addAction(startAction);

    QAction* stopAction = new QAction("Stop", this);
    connect(stopAction, &QAction::triggered, this, [this, index]() { runcontrol_.sendQCommand(index, "stop"); });
    contextMenu->addAction(stopAction);

    QAction* terminateAction = new QAction("Shutdown", this);
    connect(terminateAction, &QAction::triggered, this, [this, index]() { runcontrol_.sendQCommand(index, "shutdown"); });
    contextMenu->addAction(terminateAction);

    // Draw separator
    contextMenu->addSeparator();

    // Request possible commands from remote:
    auto dict = runcontrol_.getQCommands(index);
    for(const auto& [key, value] : dict) {
        // Filter out transition commands to not list them twice
        if(magic_enum::enum_cast<CSCP::TransitionCommand>(key, magic_enum::case_insensitive).has_value()) {
            continue;
        }

        QAction* action = new QAction(QString::fromStdString(key), this);
        connect(action, &QAction::triggered, this, [this, index, key]() {
            auto response = runcontrol_.sendQCommand(index, key);
            if(response.has_value()) {
                QMessageBox::information(NULL, "Satellite Response", QString::fromStdString(response.value()));
            }
        });
        contextMenu->addAction(action);
    }

    contextMenu->exec(viewConn->viewport()->mapToGlobal(point));
}

std::map<std::string, Controller::CommandPayload> MissionControl::parseConfigFile(QString file) {
    try {
        auto dictionaries = ConfigParser::getDictionariesFromFile(runcontrol_.getConnections(), file.toStdString());
        // Convert to CommandPayloads:
        std::map<std::string, Controller::CommandPayload> payloads;
        for(const auto& [key, dict] : dictionaries) {
            payloads.emplace(key, dict);
        }
        return payloads;
    } catch(ControllerError& err) {
        QMessageBox::warning(NULL, "ERROR", QString::fromStdString(std::string("Parsing failed: ") + err.what()));
        return {};
    }
}

Controller::CommandPayload MissionControl::parseConfigFile(QString file, const QModelIndex& index) {
    auto name = runcontrol_.getQName(index);
    try {
        auto dictionary = ConfigParser::getDictionaryFromFile(name, file.toStdString());
        if(dictionary.has_value()) {
            return {dictionary.value()};
        }
    } catch(ControllerError& err) {
        QMessageBox::warning(NULL, "ERROR", QString::fromStdString(std::string("Parsing failed: ") + err.what()));
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

    QCoreApplication::setOrganizationName("Constellation");
    QCoreApplication::setOrganizationDomain("constellation.pages.desy.de");
    QCoreApplication::setApplicationName("MissionControl");

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
        chirp_manager = std::make_unique<chirp::Manager>(brd_addr, any_addr, group_name, controller_name);
        chirp_manager->setAsDefaultInstance();
        chirp_manager->start();
    } catch(const std::exception& error) {
        LOG(logger, CRITICAL) << "Failed to initiate network discovery: " << error.what();
    }

    // Register CMDP in CHIRP and set sender name for CMDP
    SinkManager::getInstance().enableCMDPSending(controller_name);

    MissionControl gui(controller_name, group_name);
    gui.Exec();
    return 0;
}
