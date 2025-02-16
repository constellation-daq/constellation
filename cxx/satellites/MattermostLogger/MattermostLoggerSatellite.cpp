/**
 * @file
 * @brief Implementation of the MattermostLogger satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "MattermostLoggerSatellite.hpp"

#include <sstream>
#include <string_view>

#include <cpr/cpr.h>
#include <sys/socket.h>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/listener/LogListener.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::config;
using namespace constellation::listener;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::protocol;
using namespace constellation::satellite;
using namespace constellation::utils;

MattermostLoggerSatellite::MattermostLoggerSatellite(std::string_view type, std::string_view name)
    : Satellite(type, name), LogListener("MATTERMOST", [this](CMDP1Message&& msg) { log_callback(std::move(msg)); }) {}

void MattermostLoggerSatellite::initializing(Configuration& config) {
    webhook_url_ = config.get<std::string>("webhook_url");
    send_message(getCanonicalName() + " connected as logger");
    LOG(STATUS) << "Connected to Mattermost";

    const auto log_level = config.get<Level>("log_level", Level::WARNING);
    setGlobalLogLevel(log_level);
    LOG(STATUS) << "Set log level to " << log_level;

    // TODO: can startPool be called more than once safely?
    startPool();
}

void MattermostLoggerSatellite::starting(std::string_view run_identifier) {
    send_message("@channel Run " + std::string(run_identifier) + " started");
}

void MattermostLoggerSatellite::stopping() {
    send_message("@channel Run " + std::string(getRunIdentifier()) + " stopped");
}

void MattermostLoggerSatellite::interrupting(CSCP::State previous_state) {
    send_message("@channel Interrupted! Previous state: " + std::string(enum_name(previous_state)));
}

void MattermostLoggerSatellite::failure(CSCP::State /*previous_state*/) {
    stopPool();
}

// TODO: how to handle errors? do we need to trigger a failure ourselves?

void MattermostLoggerSatellite::log_callback(const CMDP1LogMessage& msg) {
    std::ostringstream msg_formatted {};
    msg_formatted << "**" << msg.getLogLevel() << "** from **" << msg.getHeader().getSender() << "** on topic **"
                  << msg.getLogTopic() << "**:\n"
                  << msg.getLogMessage();
    // TODO: add @channel for warning and critical?
    send_message(msg_formatted.str());
}

void MattermostLoggerSatellite::send_message(std::string&& message) {
    // TODO: escape " in message
    const auto response = cpr::Post(cpr::Url(webhook_url_),
                                    cpr::Header({{"Content-Type", "application/json"}}),
                                    cpr::Body({R"({"text": ")" + std::move(message) + "\"}"}));
    if(response.status_code != 200) [[unlikely]] {
        throw CommunicationError("Failed to send message to Mattermost");
    }
}
