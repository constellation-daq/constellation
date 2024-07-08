#include "euRun.hpp"

#include <chrono>
#include <format>
#include <fstream>
#include <iostream>

#include <argparse/argparse.hpp>
#include <QApplication>
#include <QDateTime>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/logging/log.hpp"
#include "constellation/core/logging/SinkManager.hpp"
#include "constellation/core/utils/casts.hpp"

using namespace constellation;
using namespace constellation::chirp;
using namespace constellation::controller;
using namespace constellation::log;
using namespace constellation::satellite;
using namespace constellation::utils;

RunControlGUI::RunControlGUI(std::string_view controller_name, std::string_view group_name)
    : QMainWindow(), runcontrol_(controller_name), logger_("GUI"), user_logger_("OP"), m_display_col(0), m_display_row(0) {
    m_map_label_str = {{"RUN", "Run"}, {"DUR", "Duration"}};
    qRegisterMetaType<QModelIndex>("QModelIndex");
    setupUi(this);

    cnstlnName->setText(QString::fromStdString("<font color=gray><b>" + std::string(group_name) + "</b></font>"));
    labelState->setText(state_str_.at(State::NEW));

    for(const auto& lvl : {Level::TRACE, Level::DEBUG, Level::INFO, Level::WARNING, Level::STATUS, Level::CRITICAL}) {
        comboBoxLogLevel->addItem(QString::fromStdString(utils::to_string(lvl)));
    }
    // Default to INFO
    comboBoxLogLevel->setCurrentIndex(2);

    for(auto& label_str : m_map_label_str) {
        QLabel* lblname = new QLabel(grpStatus);
        lblname->setObjectName("lbl_st_" + label_str.first);
        lblname->setText(label_str.second + ": ");
        QLabel* lblvalue = new QLabel(grpStatus);
        lblvalue->setObjectName("txt_st_" + label_str.first);
        grpGrid->addWidget(lblname, m_display_row, m_display_col * 2);
        grpGrid->addWidget(lblvalue, m_display_row, m_display_col * 2 + 1);
        m_str_label[label_str.first] = lblvalue;
        if(++m_display_col > 1) {
            ++m_display_row;
            m_display_col = 0;
        }
    }

    viewConn->setModel(&runcontrol_);
    viewConn->setItemDelegate(&m_delegate);

    viewConn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(viewConn, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onCustomContextMenu(const QPoint&)));

    QRect geom(-1, -1, 150, 200);
    QRect geom_from_last_program_run;

    QSettings settings("Constellation", "Vintage");
    settings.beginGroup("qcontrol");

    qsettings_run_id_ = settings.value("runidentifier", "run").toString();
    qsettings_run_seq_ = settings.value("runsequence", 0).toInt();
    runIdentifier->setText(qsettings_run_id_);
    runSequence->setValue(qsettings_run_seq_);

    m_lastexit_success = settings.value("successexit", 1).toUInt();
    geom_from_last_program_run.setSize(settings.value("size", geom.size()).toSize());
    geom_from_last_program_run.moveTo(settings.value("pos", geom.topLeft()).toPoint());
    // TODO: check last if last file exits. if not, use default value.
    txtConfigFileName->setText(settings.value("lastConfigFile", "config file not set").toString());
    settings.endGroup();

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

    setWindowTitle("Constellation QControl " CNSTLN_VERSION);
    connect(&m_timer_display, SIGNAL(timeout()), this, SLOT(DisplayTimer()));
    m_timer_display.start(1000); // internal update time of GUI
    btnInit->setEnabled(1);
    btnLand->setEnabled(1);
    btnConfig->setEnabled(1);
    btnLoadConf->setEnabled(1);
    btnStart->setEnabled(1);
    btnStop->setEnabled(1);
    btnReset->setEnabled(1);
    btnShutdown->setEnabled(1);
    btnLog->setEnabled(1);

    QSettings settings_output("Constellation", "Vintage");
    settings_output.beginGroup("qcontrol");
    settings_output.setValue("successexit", 0);
    settings_output.endGroup();
}

void RunControlGUI::on_btnInit_clicked() {
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

void RunControlGUI::on_btnShutdown_clicked() {
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

void RunControlGUI::on_btnConfig_clicked() {
    auto responses = runcontrol_.sendCommands("launch");
    for(auto& response : responses) {
        LOG(logger_, DEBUG) << "Launch: " << response.first << ": " << utils::to_string(response.second.getVerb().first);
    }
}

void RunControlGUI::on_btnLand_clicked() {
    auto responses = runcontrol_.sendCommands("land");
    for(auto& response : responses) {
        LOG(logger_, DEBUG) << "Land: " << response.first << ": " << utils::to_string(response.second.getVerb().first);
    }
}

void RunControlGUI::on_btnStart_clicked() {
    qsettings_run_id_ = runIdentifier->text();
    qsettings_run_seq_ = runSequence->value();
    if(!qsettings_run_id_.isEmpty()) {
        current_run_ = qsettings_run_id_ + "_";
    } else {
        current_run_.clear();
    }
    current_run_ += QString::number(qsettings_run_seq_);

    // FIXME run number
    auto responses = runcontrol_.sendCommands("start", current_run_.toStdString());

    // Start timer for this run
    run_timer_.start();

    for(auto& response : responses) {
        LOG(logger_, DEBUG) << "Start: " << response.first << ": " << utils::to_string(response.second.getVerb().first);
    }
}

void RunControlGUI::on_btnStop_clicked() {
    auto responses = runcontrol_.sendCommands("stop");
    for(auto& response : responses) {
        LOG(logger_, DEBUG) << "Stop: " << response.first << ": " << utils::to_string(response.second.getVerb().first);
    }

    // Invalidate run timer:
    run_timer_.invalidate();

    // Increment run sequence:
    qsettings_run_seq_++;
    runSequence->setValue(qsettings_run_seq_);
}

void RunControlGUI::on_btnReset_clicked() {
    auto responses = runcontrol_.sendCommands("recover");
    for(auto& response : responses) {
        LOG(logger_, DEBUG) << "Recover: " << response.first << ": " << utils::to_string(response.second.getVerb().first);
    }
}

void RunControlGUI::on_btnLog_clicked() {
    const auto msg = txtLogmsg->text().toStdString();
    const auto level = static_cast<Level>(comboBoxLogLevel->currentIndex());
    LOG(user_logger_, level) << msg;
    txtLogmsg->clear();
}

void RunControlGUI::on_btnLoadConf_clicked() {
    QString usedpath = QFileInfo(txtConfigFileName->text()).path();
    QString filename =
        QFileDialog::getOpenFileName(this, tr("Open File"), usedpath, tr("Configurations (*.conf *.toml *.ini)"));
    if(!filename.isNull()) {
        txtConfigFileName->setText(filename);
    }
}

void RunControlGUI::DisplayTimer() {
    auto state = updateInfos();
    updateStatusDisplay();
}

State RunControlGUI::updateInfos() {

    // FIXME revisit what needs to be done here. Most infos are updated in the background by the controller via heartbeats!
    // We might need to handle metrics here and call addStatusDisoplay and removeStatusDisplay.

    auto state = runcontrol_.getLowestState();

    QRegExp rx_conf(".+(\\.conf$|\\.ini$|\\.toml$)");
    bool confLoaded = rx_conf.exactMatch(txtConfigFileName->text());

    btnInit->setEnabled((state == State::NEW || state == State::INIT || state == State::ERROR || state == State::SAFE) &&
                        confLoaded);
    btnLand->setEnabled(state == State::ORBIT);
    btnConfig->setEnabled(state == State::INIT);
    btnLoadConf->setEnabled(state != State::RUN || state != State::ORBIT);
    btnStart->setEnabled(state == State::ORBIT);
    btnStop->setEnabled(state == State::RUN);
    btnReset->setEnabled(state == State::SAFE);
    btnShutdown->setEnabled(state == State::SAFE || state == State::INIT || state == State::NEW);

    labelState->setText(state_str_.at(state));

    auto stored_run = qsettings_run_id_ + "_" + QString::number(qsettings_run_seq_);
    if(stored_run != current_run_) {
        current_run_ = stored_run;
        QSettings settings("Constellation", "Vintage");
        settings.beginGroup("qcontrol");
        settings.setValue("runidentifier", qsettings_run_id_);
        settings.setValue("runsequence", qsettings_run_seq_);
        settings.endGroup();
    }
    if(m_str_label.count("RUN")) {
        if(state == State::RUN) {
            m_str_label.at("RUN")->setText(current_run_);
        } else {
            m_str_label.at("RUN")->setText(current_run_ + " (next run)");
        }
    }

    if(m_str_label.count("DUR")) {
        // Update only when valid:
        if(run_timer_.isValid()) {
            auto duration = std::format("{:%H:%M:%S}", std::chrono::milliseconds(run_timer_.elapsed()));
            m_str_label.at("DUR")->setText(QString::fromStdString(duration));
        }
    }

    return state;
}

void RunControlGUI::closeEvent(QCloseEvent* event) {
    QSettings settings("Constellation", "Vintage");
    settings.beginGroup("qcontrol");

    settings.setValue("runidentifier", qsettings_run_id_);
    settings.setValue("runsequence", qsettings_run_seq_);

    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.setValue("lastConfigFile", txtConfigFileName->text());
    settings.setValue("successexit", 1);
    settings.endGroup();

    // Terminate the application
    event->accept();
}

void RunControlGUI::Exec() {
    show();
    if(QApplication::instance())
        QApplication::instance()->exec();
    else
        LOG(logger_, CRITICAL) << "ERROR: RUNControlGUI::EXEC\n";
}

std::map<satellite::State, QString> RunControlGUI::state_str_ = {
    {State::NEW, "<font color='gray'><b>New</b></font>"},
    {State::initializing, "<font color='gray'><b>Initializing...</b></font>"},
    {State::INIT, "<font color='gray'><b>Initialized</b></font>"},
    {State::launching, "<font color='orange'><b>Launching...</b></font>"},
    {State::landing, "<font color='orange'><b>Landing...</b></font>"},
    {State::reconfiguring, "<font color='orange'><b>Reconfiguring...</b></font>"},
    {State::ORBIT, "<font color='orange'><b>Orbiting</b></font>"},
    {State::starting, "<font color='green'><b>Starting...</b></font>"},
    {State::stopping, "<font color='green'><b>Stopping...</b></font>"},
    {State::RUN, "<font color='green'><b>Running</b></font>"},
    {State::SAFE, "<font color='red'><b>Safe Mode</b></font>"},
    {State::interrupting, "<font color='red'><b>Interrupting...</b></font>"},
    {State::ERROR, "<font color='darkred'><b>Error</b></font>"}};

void RunControlGUI::onCustomContextMenu(const QPoint& point) {
    QModelIndex index = viewConn->indexAt(point);
    if(!index.isValid()) {
        return;
    }

    QMenu* contextMenu = new QMenu(viewConn);

    // load an eventually updated file
    loadConfigFile();

    // FIXME pass configuration
    QAction* initialiseAction = new QAction("Initialize", this);
    connect(initialiseAction, &QAction::triggered, this, [this, index]() {
        runcontrol_.sendQCommand(index, "initialize", constellation::config::Dictionary());
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

    // QAction *resetAction = new QAction("Reset", this);
    // connect(resetAction, &QAction::triggered, this, [this,index]() { runcontrol_.reset(index); });
    // contextMenu->addAction(resetAction);

    QAction* terminateAction = new QAction("Shutdown", this);
    connect(terminateAction, &QAction::triggered, this, [this, index]() { runcontrol_.sendQCommand(index, "shutdown"); });
    contextMenu->addAction(terminateAction);

    // Draw separator
    contextMenu->addSeparator();

    // Request possible commands from remote:
    auto dict = runcontrol_.getQCommands(index);
    for(const auto& [key, value] : dict) {
        // Filter out transition commands to not list them twice
        if(magic_enum::enum_cast<TransitionCommand>(key, magic_enum::case_insensitive).has_value()) {
            continue;
        }

        QAction* action = new QAction(QString::fromStdString(key), this);
        connect(action, &QAction::triggered, this, [this, index, key]() { runcontrol_.sendQCommand(index, key); });
        contextMenu->addAction(action);
    }

    contextMenu->exec(viewConn->viewport()->mapToGlobal(point));
}

bool RunControlGUI::loadConfigFile() {
    std::string settings = txtConfigFileName->text().toStdString();
    QFileInfo check_file(txtConfigFileName->text());
    if(!check_file.exists() || !check_file.isFile()) {
        QMessageBox::warning(NULL, "ERROR", "Config file does not exist.");
        return false;
    }

    // FIXME read and parse config file
    return true;
}

bool RunControlGUI::addStatusDisplay(std::string satellite_name, std::string metric) {
    QString name = QString::fromStdString(satellite_name + ":" + metric);
    QString displayName = QString::fromStdString(satellite_name + ":" + metric);
    addToGrid(displayName, name);
    return true;
}

bool RunControlGUI::removeStatusDisplay(std::string satellite_name, std::string metric) {
    // remove obsolete information from disconnected Connections
    for(auto idx = 0; idx < grpGrid->count(); idx++) {
        QLabel* l = dynamic_cast<QLabel*>(grpGrid->itemAt(idx)->widget());
        if(l->objectName() == QString::fromStdString(satellite_name + ":" + metric)) {
            // Status updates are always pairs
            m_map_label_str.erase(l->objectName());
            m_str_label.erase(l->objectName());
            grpGrid->removeWidget(l);
            delete l;
            l = dynamic_cast<QLabel*>(grpGrid->itemAt(idx)->widget());
            grpGrid->removeWidget(l);
            delete l;
        }
    }
    return true;
}
bool RunControlGUI::addToGrid(const QString& objectName, QString displayedName) {

    if(m_str_label.count(objectName) == 1) {
        // QMessageBox::warning(NULL,"ERROR - Status display","Duplicating display entry request: "+objectName);
        return false;
    }
    if(displayedName == "")
        displayedName = objectName;
    QLabel* lblname = new QLabel(grpStatus);
    lblname->setObjectName(objectName);
    lblname->setText(displayedName + ": ");
    QLabel* lblvalue = new QLabel(grpStatus);
    lblvalue->setObjectName("val_" + objectName);
    lblvalue->setText("val_" + objectName);

    int colPos = 0, rowPos = 0;
    if(2 * (m_str_label.size() + 1) < static_cast<size_t>(grpGrid->rowCount() * grpGrid->columnCount())) {
        colPos = m_display_col;
        rowPos = m_display_row;
        if(++m_display_col > 1) {
            ++m_display_row;
            m_display_col = 0;
        }
    } else {
        colPos = m_display_col;
        rowPos = m_display_row;
        if(++m_display_col > 1) {
            ++m_display_row;
            m_display_col = 0;
        }
    }
    m_map_label_str.insert(std::pair<QString, QString>(objectName, objectName + ": "));
    m_str_label.insert(std::pair<QString, QLabel*>(objectName, lblvalue));
    grpGrid->addWidget(lblname, rowPos, colPos * 2);
    grpGrid->addWidget(lblvalue, rowPos, colPos * 2 + 1);
    return true;
}
/**
 * @brief RunControlGUI::updateStatusDisplay
 * @return true if success, false otherwise (cannot happen currently)
 */
bool RunControlGUI::updateStatusDisplay() {
    // FIXME update status display with tags
    return true;
}

bool RunControlGUI::addAdditionalStatus(std::string info) {
    std::vector<std::string> results;
    std::stringstream sts(info);
    std::string token;
    while(std::getline(sts, token, ',')) {
        results.push_back(token);
    }

    if(results.size() % 2 != 0) {
        QMessageBox::warning(NULL, "ERROR", "Additional Status Display inputs are not correctly formatted - please check");
        return false;
    } else {
        for(std::size_t c = 0; c < results.size(); c += 2) {
            // check if the connection exists, otherwise do not display

            // addToGrid(QString::fromStdString(results.at(c) + ":" + results.at(c + 1)));

            // if(!found) {
            // QMessageBox::warning(
            // NULL, "ERROR", QString::fromStdString("Element \"" + results.at(c) + "\" is not connected"));
            // return false;
            // }
        }
    }
    return true;
}

std::map<std::string, Controller::CommandPayload> RunControlGUI::parseConfigFile(QString file) {
    QFileInfo check_file(file);
    if(!check_file.exists() || !check_file.isFile()) {
        QMessageBox::warning(NULL, "ERROR", "Configuration file does not exist.");
        return {};
    }

    return {};
}

/**
 * @brief RunControlGUI::allConnectionsInState
 * @param state to be checked
 * @return true if all connections are in state, false otherwise
 */
bool RunControlGUI::allConnectionsInState(constellation::satellite::State state) {
    return runcontrol_.isInState(state);
}

// NOLINTNEXTLINE(*-avoid-c-arrays)
void parse_args(int argc, char* argv[], argparse::ArgumentParser& parser) {
    // Controller name (-n)
    parser.add_argument("-n", "--name").help("controller name").default_value("qruncontrol");

    // Constellation group (-g)
    parser.add_argument("-g", "--group").help("group name").required();

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

    // Get the default logger
    auto& logger = Logger::getDefault();

    // CLI parsing
    argparse::ArgumentParser parser {"euRun", CNSTLN_VERSION};
    try {
        parse_args(argc, argv, parser);
    } catch(const std::exception& error) {
        LOG(logger, CRITICAL) << "Argument parsing failed: " << error.what();
        LOG(logger, CRITICAL) << "Run \""
                              << "euRun"
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
    SinkManager::getInstance().setGlobalConsoleLevel(default_level.value());

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

    // Create CHIRP manager and set as default
    std::unique_ptr<chirp::Manager> chirp_manager {};
    try {
        chirp_manager = std::make_unique<chirp::Manager>(brd_addr, any_addr, parser.get("group"), controller_name);
        chirp_manager->setAsDefaultInstance();
        chirp_manager->start();
    } catch(const std::exception& error) {
        LOG(logger, CRITICAL) << "Failed to initiate network discovery: " << error.what();
    }

    // Register CMDP in CHIRP and set sender name for CMDP
    SinkManager::getInstance().enableCMDPSending(controller_name);

    RunControlGUI gui(controller_name, parser.get("group"));
    gui.Exec();
    return 0;
}
