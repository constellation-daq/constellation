#include "euRun.hpp"

#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <QApplication>
#include <QDateTime>
#include <QSpinBox>
#include <string>

#include <argparse/argparse.hpp>

#include "constellation/controller/ConfigParser.hpp"
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

RunControlGUI::RunControlGUI(std::string_view controller_name, std::string_view group_name)
    : QMainWindow(), runcontrol_(controller_name), logger_("GUI"), user_logger_("OP"), m_display_col(0), m_display_row(0) {

    qRegisterMetaType<QModelIndex>("QModelIndex");
    setupUi(this);

    // Set initial values for header bar
    cnstlnName->setText(QString::fromStdString("<font color=gray><b>" + std::string(group_name) + "</b></font>"));
    labelState->setText(state_str_.at(runcontrol_.getLowestState()));
    labelNrSatellites->setText("<font color='gray'><b>" + QString::number(runcontrol_.getConnections().size()) +
                               "</b></font>");

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

    sorting_proxy_.setSourceModel(&runcontrol_);
    viewConn->setModel(&sorting_proxy_);
    viewConn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(viewConn, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onCustomContextMenu(const QPoint&)));

    // Pick up latest run identifier information - either from running Constellation or from settings
    auto run_id = runcontrol_.getRunIdentifier();
    if(run_id.empty()) {
        update_run_identifier(gui_settings_.value("run/identifier", "run").toString(),
                              gui_settings_.value("run/sequence", 0).toInt());
    } else {
        // Attempt to find sequence:
        std::size_t pos = run_id.find_last_of("_");
        // FIXME check for invalid_argument

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
    connect(&m_timer_display, SIGNAL(timeout()), this, SLOT(DisplayTimer()));
    m_timer_display.start(300); // internal update time of GUI

    // Connect run identifier edit boxes:
    connect(runIdentifier, &QLineEdit::editingFinished, this, [&]() {
        update_run_identifier(runIdentifier->text(), runSequence->value());
    });
    connect(runSequence, &QSpinBox::valueChanged, this, [&](int i) { update_run_identifier(runIdentifier->text(), i); });

    // Connect connection update signal:
    connect(&runcontrol_, &QRunControl::connectionsChanged, this, [&](std::size_t num) {
        labelNrSatellites->setText("<font color='gray'><b>" + QString::number(num) + "</b></font>");
    });

    gui_settings_.setValue("successexit", 0);
}

void RunControlGUI::update_run_identifier(const QString& text, int number) {

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
    runSequence->setValue(runSequence->value() + 1);
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

CSCP::State RunControlGUI::updateInfos() {

    // FIXME revisit what needs to be done here. Most infos are updated in the background by the controller via heartbeats!
    // We might need to handle metrics here and call addStatusDisoplay and removeStatusDisplay.

    auto state = runcontrol_.getLowestState();

    QRegularExpression rx_conf(".+(\\.conf$|\\.ini$|\\.toml$)");
    auto m = rx_conf.match(txtConfigFileName->text());

    btnInit->setEnabled((state == CSCP::State::NEW || state == CSCP::State::INIT || state == CSCP::State::ERROR ||
                         state == CSCP::State::SAFE) &&
                        m.hasMatch());
    btnReset->setEnabled(state == CSCP::State::SAFE && m.hasMatch());

    btnLand->setEnabled(state == CSCP::State::ORBIT);
    btnConfig->setEnabled(state == CSCP::State::INIT);
    btnLoadConf->setEnabled(state != CSCP::State::RUN || state != CSCP::State::ORBIT);
    btnStart->setEnabled(state == CSCP::State::ORBIT);
    btnStop->setEnabled(state == CSCP::State::RUN);
    btnShutdown->setEnabled(state == CSCP::State::SAFE || state == CSCP::State::INIT || state == CSCP::State::NEW);

    // Deactivate run identifier fields during run:
    runIdentifier->setEnabled(state != CSCP::State::RUN && state != CSCP::State::starting && state != CSCP::State::stopping);
    runSequence->setEnabled(state != CSCP::State::RUN && state != CSCP::State::starting && state != CSCP::State::stopping);

    // Update state display
    labelState->setText(state_str_.at(state));

    // Update run timer:
    if(run_timer_.isValid()) {
        auto duration =
            std::format("{:%H:%M:%S}",
                        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::milliseconds(run_timer_.elapsed())));
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

    return state;
}

void RunControlGUI::closeEvent(QCloseEvent* event) {
    gui_settings_.setValue("window/size", size());
    gui_settings_.setValue("window/pos", pos());
    gui_settings_.setValue("run/configfile", txtConfigFileName->text());
    gui_settings_.setValue("successexit", 1);

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

std::map<CSCP::State, QString> RunControlGUI::state_str_ = {
    {CSCP::State::NEW, "<font color='gray'><b>New</b></font>"},
    {CSCP::State::initializing, "<font color='gray'><b>Initializing...</b></font>"},
    {CSCP::State::INIT, "<font color='gray'><b>Initialized</b></font>"},
    {CSCP::State::launching, "<font color='orange'><b>Launching...</b></font>"},
    {CSCP::State::landing, "<font color='orange'><b>Landing...</b></font>"},
    {CSCP::State::reconfiguring, "<font color='orange'><b>Reconfiguring...</b></font>"},
    {CSCP::State::ORBIT, "<font color='orange'><b>Orbiting</b></font>"},
    {CSCP::State::starting, "<font color='green'><b>Starting...</b></font>"},
    {CSCP::State::stopping, "<font color='green'><b>Stopping...</b></font>"},
    {CSCP::State::RUN, "<font color='green'><b>Running</b></font>"},
    {CSCP::State::SAFE, "<font color='red'><b>Safe Mode</b></font>"},
    {CSCP::State::interrupting, "<font color='red'><b>Interrupting...</b></font>"},
    {CSCP::State::ERROR, "<font color='darkred'><b>Error</b></font>"}};

void RunControlGUI::onCustomContextMenu(const QPoint& point) {
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

    try {
        auto connections = runcontrol_.getConnections();
        ConfigParser parser(check_file.canonicalFilePath().toStdString(), connections);
        auto dictionaries = parser.getAll();

        // Convert to CommandPayloads:
        std::map<std::string, Controller::CommandPayload> payloads;
        for(const auto& [key, dict] : dictionaries) {
            payloads.emplace(key, dict);
        }
        return payloads;
    } catch(std::invalid_argument& err) {
        QMessageBox::warning(NULL, "ERROR", QString::fromStdString(std::string("Parsing failed: ") + err.what()));
        return {};
    }
}

Controller::CommandPayload RunControlGUI::parseConfigFile(QString file, const QModelIndex& index) {
    auto payloads = parseConfigFile(file);
    auto name = runcontrol_.getQName(index);
    return payloads[name];
}

/**
 * @brief RunControlGUI::allConnectionsInState
 * @param state to be checked
 * @return true if all connections are in state, false otherwise
 */
bool RunControlGUI::allConnectionsInState(CSCP::State state) {
    return runcontrol_.isInState(state);
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

    RunControlGUI gui(controller_name, group_name);
    gui.Exec();
    return 0;
}
