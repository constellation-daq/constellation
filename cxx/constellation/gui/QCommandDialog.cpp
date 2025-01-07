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

#include "ui_QCommandDialog.h"

using namespace constellation::config;

void QCommandParameters::add(const Value& value) {
    beginInsertRows(QModelIndex(), static_cast<int>(size()), static_cast<int>(size()));
    push_back(value);
    endInsertRows();
}

void QCommandParameters::reset() {
    beginRemoveRows(QModelIndex(), 0, size() - 1);
    List::clear();
    endRemoveRows();
}

QVariant QCommandParameters::data(const QModelIndex& index, int role) const {

    if(role != Qt::DisplayRole || !index.isValid() || index.row() >= static_cast<int>(size()) || index.column() > 0) {
        return QVariant();
    }

    return QString::fromStdString(at(index.row()).str());
}

QCommandDialog::QCommandDialog(QWidget* parent, const std::string& command, const std::string& description)
    : QDialog(parent), ui(new Ui::QCommandDialog) {
    ui->setupUi(this);
    setSizeGripEnabled(true);
    ui->commandDescription->setVisible(false);
    setWindowTitle("Satellite Command");

    ui->listView->setModel(&parameters_);

    // Connect comboBox:
    connect(ui->comboBoxType, &QComboBox::currentIndexChanged, ui->stackedWidgetType, &QStackedWidget::setCurrentIndex);
    connect(ui->btnClearParams, &QPushButton::clicked, this, [&]() { parameters_.reset(); });

    // Set command and description if provided
    if(!command.empty()) {
        ui->commandLineEdit->setText(QString::fromStdString(command));
        ui->commandLineEdit->setReadOnly(true);
    }
    if(!description.empty()) {
        ui->commandDescription->setText(QString::fromStdString(description));
        ui->commandDescription->setReadOnly(true);
        ui->commandDescription->setVisible(true);
    }
}

QCommandDialog::~QCommandDialog() {
    delete ui;
}

std::string QCommandDialog::getCommand() const {
    return ui->commandLineEdit->text().toStdString();
}

constellation::controller::Controller::CommandPayload QCommandDialog::getPayload() const {
    if(parameters_.empty()) {
        return std::monostate();
    }
    return parameters_;
}

void QCommandDialog::on_btnAddParameter_clicked() {
    // Get currently selected type:
    if(ui->stackedWidgetType->currentIndex() == 0) {
        parameters_.add(ui->doubleSpinBox->value());
        ui->doubleSpinBox->clear();
    } else if(ui->stackedWidgetType->currentIndex() == 1) {
        parameters_.add(ui->intSpinBox->value());
        ui->intSpinBox->clear();
    } else if(ui->stackedWidgetType->currentIndex() == 2) {
        parameters_.add(ui->lineEditParam->text().toStdString());
        ui->lineEditParam->clear();
    } else {
        parameters_.add(ui->checkBoxParam->checkState() == Qt::Checked);
        ui->checkBoxParam->setCheckState(Qt::Unchecked);
    }
}
