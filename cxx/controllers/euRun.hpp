#include "QRunControl.hpp"
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

private:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void on_btnInit_clicked();
    void on_btnConfig_clicked();
    void on_btnStart_clicked();
    void on_btnStop_clicked();
    void on_btnReset_clicked();
    void on_btnTerminate_clicked();
    void on_btnLog_clicked();
    void on_btnLoadConf_clicked();
    void onCustomContextMenu(const QPoint& point);

private:
    constellation::satellite::State updateInfos();

    bool loadConfigFile();
    bool addStatusDisplay(std::string satellite_name, std::string metric);
    bool removeStatusDisplay(std::string satellite_name, std::string metric);
    bool updateStatusDisplay();
    bool addToGrid(const QString& objectName, QString displayedName = "");
    bool addAdditionalStatus(std::string info);
    bool checkFile(QString file, QString usecase);

    bool allConnectionsInState(constellation::satellite::State state);

    QRunControl runcontrol_;
    constellation::log::Logger logger_;
    constellation::log::Logger user_logger_;
    std::uint32_t current_run_nr_;

    static std::map<constellation::satellite::State, QString> state_str_;
    std::map<QString, QString> m_map_label_str;

    QItemDelegate m_delegate;
    std::map<QString, QLabel*> m_str_label;

    uint32_t m_run_n_qsettings;
    int m_display_col, m_display_row;
    QMenu* contextMenu;
    bool m_lastexit_success;

    std::string m_config_at_run_path;
};
