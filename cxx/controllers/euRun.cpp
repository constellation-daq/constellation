#include "euRun.hpp"

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
using namespace constellation::log;
using namespace constellation::satellite;
using namespace constellation::utils;

RunControlGUI::RunControlGUI(td::string_view controller_name)
    : QMainWindow(0, 0), runcontrol_(controller_name), logger_("GUI"), user_logger_("USER"), m_display_col(0),
      m_scan_active(false), m_scan_interrupt_received(false), m_save_config_at_run_start(true), m_display_row(0),
      m_config_at_run_path("") {
    m_map_label_str = {{"RUN", "Run Number"}};
    qRegisterMetaType<QModelIndex>("QModelIndex");
    setupUi(this);

    lblCurrent->setText(state_str_.at(State::NEW));
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
    QSettings settings("Constellation collaboration", "Constellation");
    settings.beginGroup("qcontrol");
    m_run_n_qsettings = settings.value("runnumber", 0).toUInt();
    m_lastexit_success = settings.value("successexit", 1).toUInt();
    geom_from_last_program_run.setSize(settings.value("size", geom.size()).toSize());
    geom_from_last_program_run.moveTo(settings.value("pos", geom.topLeft()).toPoint());
    // TODO: check last if last file exits. if not, use default value.
    txtConfigFileName->setText(settings.value("lastConfigFile", "config file not set").toString());
    txtScanFile->setText(settings.value("lastScanFile", "scan file not set").toString());
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
    connect(&m_scanningTimer, SIGNAL(timeout()), this, SLOT(nextStep()));
    m_timer_display.start(1000); // internal update time of GUI
    btnInit->setEnabled(1);
    btnConfig->setEnabled(1);
    btnLoadInit->setEnabled(1);
    btnLoadConf->setEnabled(1);
    btnStart->setEnabled(1);
    btnStop->setEnabled(1);
    btnReset->setEnabled(1);
    btnTerminate->setEnabled(1);
    btnLog->setEnabled(1);

    QSettings settings_output("Constellation collaboration", "Constellation");
    settings_output.beginGroup("qcontrol");
    settings_output.setValue("successexit", 0);
    settings_output.endGroup();
}

void RunControlGUI::on_btnInit_clicked() {
    std::string settings = txtConfigFileName->text().toStdString();
    if(!checkFile(QString::fromStdString(settings), QString::fromStdString("config file")))
        return;

    auto responses = runcontrol_.sendCommand("initialize");
    for(auto& response : responses) {
        LOG(logger_, STATUS) << "Initialize: " << response.first << ": "
                             << utils::to_string(response.second.getVerb().first);
    }
}

void RunControlGUI::on_btnTerminate_clicked() {
    close();
}

void RunControlGUI::on_btnConfig_clicked() {
    auto responses = runcontrol_.sendCommand("launch");
    for(auto& response : responses) {
        LOG(logger_, STATUS) << "Launch: " << response.first << ": " << utils::to_string(response.second.getVerb().first);
    }
}

void RunControlGUI::on_btnStart_clicked() {
    QString qs_next_run = txtNextRunNumber->text();
    if(!qs_next_run.isEmpty()) {
        bool succ;
        uint32_t run_n = qs_next_run.toInt(&succ);
        if(succ) {
            // FIXME use Run nr
        }
        txtNextRunNumber->clear();
    }

    // FIXME run number
    auto responses = runcontrol_.sendCommand("start");
    for(auto& response : responses) {
        LOG(logger_, STATUS) << "Start: " << response.first << ": " << utils::to_string(response.second.getVerb().first);
    }

    if(m_save_config_at_run_start)
        store_config();
}

void RunControlGUI::on_btnStop_clicked() {
    auto responses = runcontrol_.sendCommand("stop");
    for(auto& response : responses) {
        LOG(logger_, STATUS) << "Stop: " << response.first << ": " << utils::to_string(response.second.getVerb().first);
    }
}

void RunControlGUI::on_btnReset_clicked() {
    // FIXME reset?
}

void RunControlGUI::on_btnLog_clicked() {
    std::string msg = txtLogmsg->text().toStdString();
    LOG(user_logger_, INFO) << msg;
}

void RunControlGUI::on_btnLoadConf_clicked() {
    QString usedpath = QFileInfo(txtConfigFileName->text()).path();
    QString filename = QFileDialog::getOpenFileName(this, tr("Open File"), usedpath, tr("*.conf (*.conf)"));
    if(!filename.isNull()) {
        txtConfigFileName->setText(filename);
    }
}

void RunControlGUI::DisplayTimer() {
    auto state = updateInfos();
    updateStatusDisplay();
    if(state == State::RUN)
        updateProgressBar();

    if(!m_scan.scanIsTimeBased() && m_scan_active == true)
        if(checkEventsInStep())
            nextStep();
}

State RunControlGUI::updateInfos() {

    // FIXME revisit what needs to be done here. Most infos are updated in the background by the controller via heartbeats!
    // We might need to handle metrics here and call addStatusDisoplay and removeStatusDisplay.

    auto state = runcontrol_.getLowestState();

    QRegExp rx_init(".+(\\.ini$)");
    QRegExp rx_conf(".+(\\.conf$)");
    bool confLoaded = rx_conf.exactMatch(txtConfigFileName->text());
    bool initLoaded = rx_init.exactMatch(txtInitFileName->text());

    btnInit->setEnabled((state == State::NEW || state == State::ERROR) && confLoaded);
    btnConfig->setEnabled(state == State::NEW || state == State::INIT);
    btnLoadConf->setEnabled(state != State::RUN || state != State::ORBIT);
    btnStart->setEnabled(state == State::ORBIT);
    btnStop->setEnabled(state == State::RUN && !m_scan_active);
    // FIXME
    // btnReset->setEnabled(state != State::RUN);
    // btnTerminate->setEnabled(state != State::STATE_RUNNING);

    lblCurrent->setText(state_str_.at(state));

    if(m_run_n_qsettings != current_run_nr_) {
        m_run_n_qsettings = current_run_nr_;
        QSettings settings("Constellation collaboration", "Constellation");
        settings.beginGroup("qcontrol");
        settings.setValue("runnumber", m_run_n_qsettings);
        settings.endGroup();
    }
    if(m_str_label.count("RUN")) {
        if(state == State::RUN) {
            m_str_label.at("RUN")->setText(QString::number(current_run_nr_));
        } else {
            m_str_label.at("RUN")->setText(QString::number(current_run_nr_) + " (next run)");
        }
    }

    return state;
}

void RunControlGUI::closeEvent(QCloseEvent* event) {
    if(QMessageBox::question(
           this, "Quitting", "Terminate all connections and quit?", QMessageBox::Ok | QMessageBox::Cancel) ==
       QMessageBox::Cancel) {
        event->ignore();
    } else {
        QSettings settings("Constellation collaboration", "Constellation");
        settings.beginGroup("qcontrol");
        if(current_run_nr_ != 0)
            settings.setValue("runnumber", current_run_nr_);
        else
            settings.setValue("runnumber", m_run_n_qsettings);
        settings.setValue("size", size());
        settings.setValue("pos", pos());
        settings.setValue("lastConfigFile", txtConfigFileName->text());
        settings.setValue("lastInitFile", txtInitFileName->text());
        settings.setValue("lastScanFile", txtScanFile->text());
        settings.setValue("successexit", 1);
        settings.endGroup();

        // FIXME terminate the application, send shutdown command to satellites?
        event->accept();
    }
}

void RunControlGUI::Exec() {
    show();
    if(QApplication::instance())
        QApplication::instance()->exec();
    else
        LOG(logger_, CRITICAL) << "ERROR: RUNControlGUI::EXEC\n";
}

std::map<satellite::State, QString> RunControlGUI::state_str_ = {
    {State::NEW, "<font size=12 color='red'><b>Current State: New </b></font>"},
    {State::INIT, "<font size=12 color='red'><b>Current State: Initialized </b></font>"},
    {State::ORBIT, "<font size=12 color='orange'><b>Current State: Orbiting </b></font>"},
    {State::RUN, "<font size=12 color='green'><b>Current State: Running </b></font>"},
    {State::SAFE, "<font size=12 color='red'><b>Current State: Safe Mode </b></font>"},
    {State::ERROR, "<font size=12 color='darkred'><b>Current State: Error </b></font>"}};

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
    connect(initialiseAction, &QAction::triggered, this, [this, index]() { runcontrol_.sendCommand(index, "initialize"); });
    contextMenu->addAction(initialiseAction);

    // load an eventually updated config file
    QAction* launchAction = new QAction("Launch", this);
    connect(launchAction, &QAction::triggered, this, [this, index]() { runcontrol_.sendCommand(index, "launch"); });
    contextMenu->addAction(launchAction);

    QAction* landAction = new QAction("Land", this);
    connect(landAction, &QAction::triggered, this, [this, index]() { runcontrol_.sendCommand(index, "land"); });
    contextMenu->addAction(landAction);

    QAction* startAction = new QAction("Start", this);
    connect(startAction, &QAction::triggered, this, [this, index]() { runcontrol_.sendCommand(index, "start"); });
    contextMenu->addAction(startAction);

    QAction* stopAction = new QAction("Stop", this);
    connect(stopAction, &QAction::triggered, this, [this, index]() { runcontrol_.sendCommand(index, "stop"); });
    contextMenu->addAction(stopAction);

    // QAction *resetAction = new QAction("Reset", this);
    // connect(resetAction, &QAction::triggered, this, [this,index]() { runcontrol_.reset(index); });
    // contextMenu->addAction(resetAction);

    // QAction *terminateAction = new QAction("Terminate", this);
    // connect(terminateAction, &QAction::triggered, this, [this,index]() { /** FIXME end the satellite */ });
    // contextMenu->addAction(terminateAction);

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
    if(2 * (m_str_label.size() + 1) < grpGrid->rowCount() * grpGrid->columnCount()) {
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
    auto it = m_map_conn_status_last.begin();
    while(it != m_map_conn_status_last.end()) {
        // elements might not be existing at startup/being asynchronously changed
        if(it->first && it->second) {
            auto labelit = m_str_label.begin();
            while(labelit != m_str_label.end()) {
                std::string labelname = (labelit->first.toStdString()).substr(0, labelit->first.toStdString().find(":"));
                std::string displayedItem =
                    (labelit->first.toStdString())
                        .substr(labelit->first.toStdString().find(":") + 1, labelit->first.toStdString().size());
                if(it->first->GetName() == labelname) {
                    auto tags = it->second->GetTags();
                    // obviously not really elegant...
                    for(auto& tag : tags) {
                        if(tag.first == displayedItem && displayedItem == "EventN")
                            labelit->second->setText(QString::fromStdString(tag.second + " Events"));
                        else if(tag.first == displayedItem && displayedItem == "Freq. (avg.) [kHz]")
                            labelit->second->setText(QString::fromStdString(tag.second + " kHz"));
                        else if(tag.first == displayedItem)
                            labelit->second->setText(QString::fromStdString(tag.second));
                    }
                }
                labelit++;
            }
        }
        it++;
    }
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
        for(auto c = 0; c < results.size(); c += 2) {
            // check if the connection exists, otherwise do not display
            auto it = m_map_conn_status_last.begin();
            bool found = false;
            while(it != m_map_conn_status_last.end()) {
                if(it->first && it->first->GetName() == results.at(c)) {
                    addToGrid(QString::fromStdString(results.at(c) + ":" + results.at(c + 1)));
                    found = true;
                }
                it++;
            }
            if(!found) {
                QMessageBox::warning(
                    NULL, "ERROR", QString::fromStdString("Element \"" + results.at(c) + "\" is not connected"));
                return false;
            }
        }
    }
    return true;
}

bool RunControlGUI::checkFile(QString file, QString usecase) {
    QFileInfo check_file(file);
    if(!check_file.exists() || !check_file.isFile()) {
        QMessageBox::warning(NULL, "ERROR", QString(usecase + " file does not exist."));
        return false;
    } else
        return true;
}

/**
 * @brief RunControlGUI::on_btn_LoadScanFile_clicked
 * @abstract push Button to open file dialog to select the scan configuration
 * file.
 * @group Scanning utils, RunControlGUI
 */
void RunControlGUI::on_btn_LoadScanFile_clicked() {
    QString usedpath = QFileInfo(txtScanFile->text()).path();
    QString filename = QFileDialog::getOpenFileName(this, tr("Open File"), usedpath, tr("*.scan (*.scan)"));
    if(!filename.isNull()) {
        txtScanFile->setText(filename);
    }
}

/**
 * @brief RunControlGUI::on_btnStartScan_clicked
 * @abstract Button to control the scanning procedure. Does not implement any real
 * functionality, only changes status bools and texts
 *
 */
void RunControlGUI::on_btnStartScan_clicked() {
    if(m_scan_active == true) {
        QMessageBox::StandardButton reply;
        reply =
            QMessageBox::question(NULL,
                                  "Interrupt Scan",
                                  "Do you want to stop immediately?\n Hitting no will stop after finishing the current step",
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Abort);
        if(reply == QMessageBox::Yes) {
            m_scan_active = false;
            m_scanningTimer.stop();
            nextStep();
            return;
        } else if(reply == QMessageBox::Abort) {
            m_scan_active = true;
            btnStartScan->setText("Interrupt scan");
        } else if(reply == QMessageBox::No) {
            m_scan_interrupt_received = true;
            btnStartScan->setText("Scan stops after current step");
        }
    } else {
        if(!readScanConfig())
            return;
        m_scan_active = true;
        m_scan_interrupt_received = false;
        LOG(logger_, INFO) << "STARTING SCAN";
        btnStartScan->setText("Interrupt Scan");
        nextStep();
        return;
    }
}

/**
 * @brief RunControlGUI::prepareAndStartStep
 * @abstract stop the data taking, update the configuration and start a new run
 * @return Returns true if step has been successful
 */
void RunControlGUI::nextStep() {
    if(!m_scan_active) {
        btnStartScan->setText("Start scan");
        LOG(logger_, INFO) << "Stopping scan";
        m_scan_interrupt_received = false;
        m_scanningTimer.stop();
        if(!allConnectionsInState(State::ORBIT))
            on_btnStop_clicked();
        return;
    }
    if(m_scan.currentStep() != 0)
        on_btnStop_clicked();
    std::string conf = m_scan.nextConfig();
    LOG(logger_, INFO) << "Next file (" << m_scan.currentStep() << "): " << conf;
    if(m_scan_interrupt_received == false && m_scan_active == true && conf != "finished") {
        LOG(logger_, INFO) << "Next step";
        txtConfigFileName->setText(QString(conf.c_str()));
        QCoreApplication::processEvents();
        while((!allConnectionsInState(State::ORBIT) && m_scan.scanHasbeenStarted()) ||
              (!allConnectionsInState(State::ORBIT) && !m_scan_active)) {
            updateInfos();
            QCoreApplication::processEvents();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            LOG(logger_, INFO) << "Waiting until all components are stopped";
        }

        updateInfos();
        std::this_thread::sleep_for(std::chrono::seconds(3));
        on_btnConfig_clicked();
        while(!allConnectionsInState(State::ORBIT) && m_scan_active) {
            updateInfos();
            QCoreApplication::processEvents();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            LOG(logger_, INFO) << "Waiting until all components are (re)configured";
        }
        updateInfos();
        LOG(logger_, INFO) << "Ready for next step";

        on_btnStart_clicked();
        while(!allConnectionsInState(State::RUN)) {
            updateInfos();
            QCoreApplication::processEvents();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            LOG(logger_, INFO) << "Waiting until all components are running";
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
        updateInfos();

        if(m_scan.scanIsTimeBased()) {
            m_scanningTimer.start(1000 * m_scan.timePerStep());
            LOG(logger_, INFO) << "Time based scan next step";
        } else {
            LOG(logger_, INFO) << "Event based scan next step";
        }
        // stop the scan here
    } else {
        btnStartScan->setText("Start scan");
        m_scan_active = false;
        m_scan_interrupt_received = false;
        m_scanningTimer.stop();
    }
    m_scan.scanStarted();
    return;
}
/**
 * @brief RunControlGUI::allConnectionsInState
 * @param state to be checked
 * @return true if all connections are in state, false otherwise
 */
bool RunControlGUI::allConnectionsInState(constellation::satellite::State state) {
    return runcontrol_.isInState(state);
}

/**
 * @brief RunControlGUI::readScanConfig
 * @abstract Read the scan config file and prepare all parameters
 * @return true if successful
 */
bool RunControlGUI::readScanConfig() {
    m_scan.reset();
    return m_scan.setupScan(txtConfigFileName->text().toStdString(), txtScanFile->text().toStdString());
}
/**
 * @brief RunControlGUI::checkEventsInStep
 * @abstract check if the requested number of events for a certain step is recorded
 * @return true if reached/surpassed, false otherwise
 */
bool RunControlGUI::checkEventsInStep() {
    int events = getEventsCurrent();
    return ((events > 0 ? events : (m_scan.eventsPerStep() - 2)) > m_scan.eventsPerStep());
}

/**
 * @brief RunControlGUI::getEventsCurrent
 * @return Number of events in current step of scans
 */

int RunControlGUI::getEventsCurrent() {
    // FIXME required metrics reading
    return -1;
}

void RunControlGUI::store_config() {
    std::string configFile = txtConfigFileName->text().toStdString();
    std::string command =
        "cp " + configFile + " " + m_config_at_run_path + "config_run_" + std::to_string(current_run_nr_) + ".txt";
    system(command.c_str());
}

void RunControlGUI::updateProgressBar() {
    double scanProgress = 0;
    if(m_scan_active) {
        scanProgress = ((m_scan.currentStep() - 1) % m_scan.nSteps()) / double(std::max(1, m_scan.nSteps())) * 100;
        if(m_scan.scanIsTimeBased())
            scanProgress += ((m_scanningTimer.interval() - m_scanningTimer.remainingTime()) /
                             double(std::max(1, m_scanningTimer.interval())) * 100. / std::max(1, m_scan.nSteps()));
        else
            scanProgress += getEventsCurrent() / double(m_scan.eventsPerStep()) * 100. / std::max(1, m_scan.nSteps());
    }
    progressBar_scan->setValue(scanProgress);
}

void RunControlGUI::on_checkBox_stateChanged(int arg1) {
    m_save_config_at_run_start = arg1;
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
    asio::ip::address brd_addr {};
    try {
        brd_addr = asio::ip::address::from_string(get_arg(parser, "brd"));
    } catch(const asio::system_error& error) {
        LOG(logger, CRITICAL) << "Invalid broadcast address \"" << get_arg(parser, "brd") << "\"";
        return 1;
    }
    asio::ip::address any_addr {};
    try {
        any_addr = asio::ip::address::from_string(get_arg(parser, "any"));
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
    SinkManager::getInstance().registerService(controller_name);

    RunControlGUI gui(controller_name);
    gui.Exec();
    return 0;
}
