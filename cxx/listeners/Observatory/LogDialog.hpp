#ifndef INCLUDED_LogDialog_hh
#define INCLUDED_LogDialog_hh

#include "QLogListener.hpp"
#include "ui_LogDialog.h"

class LogDialog : public QDialog, Ui::dlgLogMessage {
public:
    LogDialog(const LogMessage& msg) {
        setupUi(this);
        for(int i = 0; i < msg.NumColumns(); ++i) {
            QTreeWidgetItem* item = new QTreeWidgetItem(treeLogMessage);
            item->setText(0, msg.ColumnName(i));
            item->setText(1, msg.Text(i).c_str());
            item->setText(2, msg[i]);
        }
        show();
    }
};

#endif // INCLUDED_LogDialog_hh
