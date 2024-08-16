#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QInputDialog>
#include <QItemDelegate>
#include <QMainWindow>
#include <QMessageBox>
#include <QPainter>
#include <QRegularExpression>
#include <QSettings>
#include <QString>
#include <QTimer>

#include "QRunControl.hpp"
#include "ui_MissionControl.h"

class RunControlGUI : public QMainWindow, public Ui::wndRun {

    Q_OBJECT
public:
    RunControlGUI(std::string_view controller_name, std::string_view group_name);

    void Exec();

private:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void updateInfos();

    void update_run_identifier(const QString& text, int number);

    void on_btnInit_clicked();
    void on_btnLand_clicked();
    void on_btnConfig_clicked();
    void on_btnStart_clicked();
    void on_btnStop_clicked();
    void on_btnShutdown_clicked();
    void on_btnLog_clicked();
    void on_btnLoadConf_clicked();
    void onCustomContextMenu(const QPoint& point);

private:
    std::map<std::string, constellation::controller::Controller::CommandPayload> parseConfigFile(QString file);
    constellation::controller::Controller::CommandPayload parseConfigFile(QString file, const QModelIndex& index);

    QRunControl runcontrol_;
    QRunControlSortProxy sorting_proxy_;
    constellation::log::Logger logger_;
    constellation::log::Logger user_logger_;

    /* Run identifier */
    QString current_run_;
    QDateTime run_start_time_;

    QString get_state_str(constellation::protocol::CSCP::State state, bool global_) const;

    QTimer m_timer_display;

    QMenu* contextMenu;
    bool m_lastexit_success;

    QSettings gui_settings_;
};
