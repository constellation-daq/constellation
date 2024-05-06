#include "QRunControl.hpp"
#include "scanHelper.hh"
#include "ui_euRun.h"

#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QInputDialog>
#include <QItemDelegate>
#include <QMainWindow>
#include <QMessageBox>
#include <QPainter>
#include <QRegExp>
#include <QSettings>
#include <QString>
#include <QTimer>

class RunControlGUI : public QMainWindow, public Ui::wndRun {

    Q_OBJECT
public:
    RunControlGUI(std::string_view controller_name);

    void Exec();
private slots:
    void on_checkBox_stateChanged(int arg1);

private:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void DisplayTimer();
    void on_btnInit_clicked();
    void on_btnConfig_clicked();
    void on_btnStart_clicked();
    void on_btnStop_clicked();
    void on_btnReset_clicked();
    void on_btnTerminate_clicked();
    void on_btnLog_clicked();
    void on_btnLoadInit_clicked();
    void on_btnLoadConf_clicked();
    void onCustomContextMenu(const QPoint& point);

    void on_btn_LoadScanFile_clicked();
    void on_btnStartScan_clicked();
    void nextStep();

private:
    constellation::satellite::State updateInfos();

    bool loadConfigFile();
    bool addStatusDisplay(std::string satellite_name, std::string metric);
    bool removeStatusDisplay(std::string satellite_name, std::string metric);
    bool updateStatusDisplay();
    bool addToGrid(const QString& objectName, QString displayedName = "");
    bool addAdditionalStatus(std::string info);
    bool checkFile(QString file, QString usecase);

    bool readScanConfig();
    bool allConnectionsInState(constellation::satellite::State state);
    bool checkEventsInStep();
    int getEventsCurrent();
    void store_config();

    constellation::log::Logger logger_;
    constellation::log::Logger user_logger_;
    QRunControl runcontrol_;

    static std::map<int, QString> m_map_state_str;
    std::map<QString, QString> m_map_label_str;

    QItemDelegate m_delegate;
    QTimer m_timer_display;
    std::map<QString, QLabel*> m_str_label;

    std::map<eudaq::ConnectionSPC, eudaq::StatusSPC> m_map_conn_status_last;

    uint32_t m_run_n_qsettings;
    int m_display_col, m_display_row;
    QMenu* contextMenu;
    bool m_lastexit_success;

    bool m_scan_active;
    bool m_scan_interrupt_received;
    bool m_save_config_at_run_start;

    QTimer m_scanningTimer;

    std::shared_ptr<eudaq::Configuration> m_scan_config;
    Scan m_scan;
    std::string m_config_at_run_path;

    void updateProgressBar();
};
