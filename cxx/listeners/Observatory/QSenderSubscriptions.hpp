/**
 * @file
 * @brief Sender Subscription Widget
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <functional>
#include <memory>

#include <QWidget>

#include "constellation/core/log/Level.hpp"

namespace Ui {
    class QSenderSubscriptions;
}

class QSenderSubscriptions : public QWidget {
    Q_OBJECT

public:
    QSenderSubscriptions(QWidget* parent,
                         const std::string& name,
                         std::function<void(const std::string&, const std::string&, constellation::log::Level)> callback);

private slots:
    /**
     * @brief Private slot for changes of the sender log level setting
     *
     * @param index New index of the log level
     */
    void on_senderLevel_currentIndexChanged(int index);

private:
    std::string name_;
    std::shared_ptr<Ui::QSenderSubscriptions> ui_;
    std::function<void(const std::string&, const std::string&, constellation::log::Level)> callback_;
};
