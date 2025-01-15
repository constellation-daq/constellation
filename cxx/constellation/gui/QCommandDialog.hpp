/**
 * @file
 * @brief Command Dialog
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <memory>
#include <string>

#include <QAbstractListModel>
#include <QDialog>

#include "constellation/build.hpp"
#include "constellation/controller/Controller.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/config/Value.hpp"

// Expose Qt class auto-generated from the user interface XML:
namespace Ui { // NOLINT(readability-identifier-naming)
    class QCommandDialog;
} // namespace Ui

namespace constellation::gui {

    class CNSTLN_API QCommandParameters : public QAbstractListModel, public config::List {
        Q_OBJECT

    public:
        QCommandParameters(QObject* parent = nullptr) : QAbstractListModel(parent) {}
        virtual ~QCommandParameters() = default;

        // No copy constructor/assignment/move constructor/assignment
        /// @cond doxygen_suppress
        QCommandParameters(const QCommandParameters& other) = delete;
        QCommandParameters& operator=(const QCommandParameters& other) = delete;
        QCommandParameters(QCommandParameters&& other) noexcept = delete;
        QCommandParameters& operator=(QCommandParameters&& other) = delete;
        /// @endcond

        /**
         * @brief Get total number of rows, i.e. number of parameters
         *
         * @return Number of parameters
         */
        int rowCount(const QModelIndex& /*unused*/) const override { return static_cast<int>(size()); }

        /**
         * @brief Retrieve the data of a given cell (column, row) of the model i.e. a specific parameter
         *
         * @param index QModelIndex to obtain the data for
         * @param role Role code
         *
         * @return QVariant holding the parameter data for the requested cell
         */
        QVariant data(const QModelIndex& index, int role) const override;

        void add(const config::Value& value);
        void reset();
    };

    /**
     * @class QCommandDialog
     * @brief Dialog window to add commands to a list
     */
    class CNSTLN_API QCommandDialog : public QDialog {
        Q_OBJECT

    public:
        explicit QCommandDialog(QWidget* parent,
                                const std::string& satellite,
                                const std::string& command = "",
                                const std::string& description = "");
        std::string getCommand() const;
        controller::Controller::CommandPayload getPayload() const;

    private slots:
        /**
         * @brief Private slot for "Add" button to enlist a parameter
         */
        void on_btnAddParameter_clicked();

    private:
        std::shared_ptr<Ui::QCommandDialog> ui_;
        QCommandParameters parameters_;
    };
} // namespace constellation::gui
