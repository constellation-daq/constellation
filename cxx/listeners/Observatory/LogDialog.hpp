/**
 * @file
 * @brief Log Dialog
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "QLogListener.hpp"
#include "ui_LogDialog.h"

class LogDialog : public QDialog, Ui::dlgLogMessage {
public:
    LogDialog(const LogMessage& msg) {
        setupUi(this);
        for(int i = 0; i < msg.NumExtendedColumns(); ++i) {
            QTreeWidgetItem* item = new QTreeWidgetItem(treeLogMessage);
            item->setText(0, msg.ColumnName(i));
            item->setText(1, msg[i]);
        }
        show();
    }
};
