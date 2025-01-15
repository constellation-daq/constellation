/**
 * @file
 * @brief Sender Subscription Implementation
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QSenderSubscriptions.hpp"

#include <functional>

#include "constellation/core/log/Level.hpp"

#include "ui_QSenderSubscriptions.h"

using namespace constellation;

QSenderSubscriptions::QSenderSubscriptions(QWidget* parent,
                                           const std::string& name,
                                           std::function<void(const std::string&, const std::string&, log::Level)> callback)
    : QWidget(parent), name_(name), ui_(new Ui::QSenderSubscriptions), callback_(std::move(callback)) {
    ui_->setupUi(this);
    ui_->senderName->setText(QString::fromStdString(name_));
}

void QSenderSubscriptions::on_senderLevel_currentIndexChanged(int index) {
    const auto lvl = log::Level(index);
    callback_(name_, "", lvl);
}
