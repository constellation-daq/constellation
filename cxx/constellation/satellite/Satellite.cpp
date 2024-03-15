/**
 * @file
 * @brief Implementation of Satellite class
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Satellite.hpp"

#include <any>
#include <stop_token>

#include "constellation/core/logging/log.hpp"

using namespace constellation::satellite;

Satellite::Satellite(std::string_view name) : logger_("SATELLITE"), name_(name) {}

void Satellite::initializing(const std::stop_token& /* stop_token */, const std::any& /* config */) {
    LOG(logger_, INFO) << "Initializing - empty";
}

void Satellite::launching(const std::stop_token& /* stop_token */) {
    LOG(logger_, INFO) << "Launching - empty";
}

void Satellite::landing(const std::stop_token& /* stop_token */) {
    LOG(logger_, INFO) << "Landing - empty";
}

void Satellite::reconfiguring(const std::stop_token& /* stop_token */, const std::any& /* partial_config */) {
    // TODO(stephan.lachnit): throw if not supported
    LOG(logger_, INFO) << "Reconfiguring - empty";
}

void Satellite::starting(const std::stop_token& /* stop_token */, std::uint32_t run_number) {
    LOG(logger_, INFO) << "Starting run " << run_number << " - empty";
}

void Satellite::stopping(const std::stop_token& /* stop_token */) {
    LOG(logger_, INFO) << "Stopping - empty";
}

void Satellite::running(const std::stop_token& /* stop_token */) {
    LOG(logger_, INFO) << "Running - empty";
}

void Satellite::interrupting(const std::stop_token& /* stop_token */, State /* previous_state */) {
    LOG(logger_, INFO) << "Interrupting - empty";
}

void Satellite::on_failure(const std::stop_token& /* stop_token */, State /* previous_state */) {
    LOG(logger_, INFO) << "Failure - empty";
}
