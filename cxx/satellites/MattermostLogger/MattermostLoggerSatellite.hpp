/**
 * @file
 * @brief MattermostLogger satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <string>
#include <string_view>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/listener/LogListener.hpp"
#include "constellation/satellite/Satellite.hpp"

class MattermostLoggerSatellite final : public constellation::satellite::Satellite, constellation::listener::LogListener {
public:
    MattermostLoggerSatellite(std::string_view type, std::string_view name);

    void initializing(constellation::config::Configuration& config) final;
    void starting(std::string_view run_identifier) final;
    void stopping() final;
    void interrupting(constellation::protocol::CSCP::State previous_state) final;
    void failure(constellation::protocol::CSCP::State previous_state) final;

private:
    void log_callback(const constellation::message::CMDP1LogMessage& msg);
    void send_message(std::string&& message);

private:
    std::string webhook_url_;
};
