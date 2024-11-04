/**
 * @file
 * @brief Data receiving and discarding satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstddef>
#include <string_view>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/utils/timers.hpp"
#include "constellation/satellite/ReceiverSatellite.hpp"

class DevNullReceiverSatellite final : public constellation::satellite::ReceiverSatellite {
public:
    DevNullReceiverSatellite(std::string_view type, std::string_view name);

    void starting(std::string_view run_identifier) final;
    void stopping() final;

protected:
    void receive_bor(const constellation::message::CDTP1Message::Header& header,
                     constellation::config::Configuration config) final;
    void receive_data(constellation::message::CDTP1Message&& data_message) final;
    void receive_eor(const constellation::message::CDTP1Message::Header& header,
                     constellation::config::Dictionary run_metadata) final;

private:
    std::size_t bytes_received_ {};
    constellation::utils::StopwatchTimer timer_;
};
