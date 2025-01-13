/**
 * @file
 * @brief Command dialog
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QCommandDialog.hpp"

#include <string>

#include <QDialog>

#include "constellation/controller/Controller.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/config/Value.hpp"

#include "ui_QCommandDialog.h"

using namespace constellation::config;
using namespace constellation::gui;

void QCommandParameters::add(const Value& value) {
    beginInsertRows(QModelIndex(), static_cast<int>(size()), static_cast<int>(size()));
    push_back(value);
    endInsertRows();
}

void QCommandParameters::reset() {
    beginRemoveRows(QModelIndex(), 0, static_cast<int>(size() - 1));
    List::clear();
    endRemoveRows();
}

QVariant QCommandParameters::data(const QModelIndex& index, int role) const {

    if(role != Qt::DisplayRole || !index.isValid() || index.row() >= static_cast<int>(size()) || index.column() > 0) {
        return {};
    }

    return QString::fromStdString(at(index.row()).str());
}

QCommandDialog::QCommandDialog(QWidget* parent, const std::string& command, const std::string& description)
    : QDialog(parent), ui_(new Ui::QCommandDialog) {
    ui_->setupUi(this);
    setSizeGripEnabled(true);
    ui_->commandDescription->setVisible(false);
    setWindowTitle("Satellite Command");

    ui_->tableView->setModel(&parameters_);

    // Connect comboBox:
    connect(ui_->comboBoxType,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            ui_->stackedWidgetType,
            &QStackedWidget::setCurrentIndex);
    connect(ui_->btnClearParams, &QPushButton::clicked, this, [&]() { parameters_.reset(); });

    // Set command and description if provided
    if(!command.empty()) {
        ui_->commandLineEdit->setText(QString::fromStdString(command));
        ui_->commandLineEdit->setReadOnly(true);
    }
    if(!description.empty()) {
        ui_->commandDescription->setText(QString::fromStdString(description));
        ui_->commandDescription->setReadOnly(true);
        ui_->commandDescription->setVisible(true);
    }
}

std::string QCommandDialog::getCommand() const {
    return ui_->commandLineEdit->text().toStdString();
}

constellation::controller::Controller::CommandPayload QCommandDialog::getPayload() const {
    if(parameters_.empty()) {
        return std::monostate();
    }
    return parameters_;
}

void QCommandDialog::on_btnAddParameter_clicked() {
    // Get currently selected type:
    if(ui_->stackedWidgetType->currentIndex() == 0) {
        parameters_.add(ui_->doubleSpinBox->value());
        ui_->doubleSpinBox->clear();
    } else if(ui_->stackedWidgetType->currentIndex() == 1) {
        parameters_.add(ui_->intSpinBox->value());
        ui_->intSpinBox->clear();
    } else if(ui_->stackedWidgetType->currentIndex() == 2) {
        parameters_.add(ui_->lineEditParam->text().toStdString());
        ui_->lineEditParam->clear();
    } else {
        parameters_.add(ui_->checkBoxParam->checkState() == Qt::Checked);
        ui_->checkBoxParam->setCheckState(Qt::Unchecked);
    }
}
