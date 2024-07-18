#include "QRunControl.hpp"
#include "ui_euRun.h"

#include <QCloseEvent>
#include <QDir>
#include <QElapsedTimer>
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
    RunControlGUI(std::string_view controller_name, std::string_view group_name);

    void Exec();

private:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void DisplayTimer();

    void on_btnInit_clicked();
    void on_btnLand_clicked();
    void on_btnConfig_clicked();
    void on_btnStart_clicked();
    void on_btnStop_clicked();
    void on_btnReset_clicked();
    void on_btnShutdown_clicked();
    void on_btnLog_clicked();
    void on_btnLoadConf_clicked();
    void onCustomContextMenu(const QPoint& point);

private:
    constellation::protocol::CSCP::State updateInfos();

    bool addStatusDisplay(std::string satellite_name, std::string metric);
    bool removeStatusDisplay(std::string satellite_name, std::string metric);
    bool updateStatusDisplay();
    bool addToGrid(const QString& objectName, QString displayedName = "");
    bool addAdditionalStatus(std::string info);
    std::map<std::string, constellation::controller::Controller::CommandPayload> parseConfigFile(QString file);
    constellation::controller::Controller::CommandPayload parseConfigFile(QString file, const QModelIndex& index);

    bool allConnectionsInState(constellation::protocol::CSCP::State state);

    QRunControl runcontrol_;
    QRunControlSortProxy sorting_proxy_;
    constellation::log::Logger logger_;
    constellation::log::Logger user_logger_;

    /* Run identifier */
    QString current_run_;
    QString qsettings_run_id_;
    int qsettings_run_seq_;

    QElapsedTimer run_timer_;

    static std::map<constellation::protocol::CSCP::State, QString> state_str_;
    std::map<QString, QString> m_map_label_str;

    QTimer m_timer_display;
    std::map<QString, QLabel*> m_str_label;

    int m_display_col, m_display_row;
    QMenu* contextMenu;
    bool m_lastexit_success;

    std::string m_config_at_run_path;
};
