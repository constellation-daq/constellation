/**
 * @file
 * @brief Connection detail dialog
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QConnectionDialog.hpp"

#include <iterator>
#include <string>
#include <string_view>

#include <QDialog>
#include <QString>

#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/gui/qt_utils.hpp"

#include "ui_QConnectionDialog.h"

using namespace constellation::config;
using namespace constellation::gui;
using namespace constellation::message;

QConnectionDialog::QConnectionDialog(QWidget* parent,
                                     const std::string& name,
                                     const QMap<QString, QVariant>& details,
                                     const config::Dictionary& commands)
    : QDialog(parent), ui_(new Ui::QConnectionDialog) {

    ui_->setupUi(this);
    setSizeGripEnabled(true);
    setWindowTitle("Satellite Connection Details");

    // Set header information:
    ui_->satelliteName->setText("<font color='gray'><b>" + QString::fromStdString(name) + "</b></font>");
    // ui_->satelliteState->setText(get_styled_state(state, true) + "</b></font>");

    // Set connection details:
    ui_->connectionTable->setRowCount(details.size());
    ui_->connectionTable->setColumnCount(2);
    ui_->connectionTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    auto it = details.cbegin();
    for(int idx = 0; idx < static_cast<int>(details.size()); idx++) {
        // QTableWidget takes ownership of assigned QTableWidgetItems
        // NOLINTBEGIN(cppcoreguidelines-owning-memory)
        ui_->connectionTable->setItem(idx, 0, new QTableWidgetItem(it.key()));
        ui_->connectionTable->setItem(idx, 1, new QTableWidgetItem(it.value().toString()));
        // NOLINTEND(cppcoreguidelines-owning-memory)
        std::advance(it, 1);
    }

    // Set commands:
    show_commands(commands);

    show();
}

void QConnectionDialog::show_commands(const Dictionary& dict) {
    ui_->commandTable->setRowCount(static_cast<int>(dict.size()));
    ui_->commandTable->setColumnCount(2);
    ui_->commandTable->setHorizontalHeaderLabels({"Command", "Description"});
    ui_->commandTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    auto it = dict.begin();
    for(int idx = 0; idx < static_cast<int>(dict.size()); idx++) {
        // QTableWidget takes ownership of assigned QTableWidgetItems
        // NOLINTBEGIN(cppcoreguidelines-owning-memory)
        ui_->commandTable->setItem(idx, 0, new QTableWidgetItem(QString::fromStdString(it->first)));
        ui_->commandTable->setItem(idx, 1, new QTableWidgetItem(QString::fromStdString(it->second.str())));
        // NOLINTEND(cppcoreguidelines-owning-memory)
        std::advance(it, 1);
    }
}
