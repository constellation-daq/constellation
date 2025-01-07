/**
 * @file
 * @brief Command Dialog
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <string>

#include <QAbstractListModel>
#include <QDialog>

#include "constellation/build.hpp"
#include "constellation/controller/Controller.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/config/Value.hpp"

namespace Ui {
    class QCommandDialog;
}

class QCommandParameters : public QAbstractListModel, public constellation::config::List {
    Q_OBJECT

public:
    QCommandParameters(QObject* parent = nullptr) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex& /*unused*/) const override { return this->size(); }
    QVariant data(const QModelIndex& index, int role) const override;

    void add(const constellation::config::Value& value);
    void reset();
};

/**
 * @class QCommandDialog
 * @brief Dialog window to add commands to a list
 */
class CNSTLN_API QCommandDialog : public QDialog {
    Q_OBJECT

public:
    explicit QCommandDialog(QWidget* parent = nullptr, const std::string& command = "", const std::string& description = "");
    virtual ~QCommandDialog();

    std::string getCommand() const;
    constellation::controller::Controller::CommandPayload getPayload() const;

private slots:
    /**
     * @brief Private slot for "Add" button to enlist a parameter
     */
    void on_btnAddParameter_clicked();

private:
    Ui::QCommandDialog* ui;
    QCommandParameters parameters_;
};
