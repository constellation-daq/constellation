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
#include <string_view>

#include <QDialog>
#include <QMap>
#include <QStandardItemModel>

#include "constellation/build.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"

// Expose Qt class auto-generated from the user interface XML:
namespace Ui { // NOLINT(readability-identifier-naming)
    class QConnectionDialog;
} // namespace Ui

namespace constellation::gui {

    /**
     * @class QConnectionDialog
     * @brief Dialog window to show satellite connection details
     */
    class CNSTLN_API QConnectionDialog : public QDialog {
        Q_OBJECT

    public:
        /**
         * @brief Constructor of the QConnectionDialog
         *
         * @param parent Parent widget of the dialog
         * @param message CSCP message returned by the satellite
         */
        explicit QConnectionDialog(QWidget* parent,
                                   const std::string& name,
                                   const QMap<QString, QVariant>& details,
                                   const config::Dictionary& commands);

    private:
        /** Helper to fill the UI with command dictionary */
        void show_commands(const config::Dictionary& dict);

    private:
        std::shared_ptr<Ui::QConnectionDialog> ui_;
    };

} // namespace constellation::gui
