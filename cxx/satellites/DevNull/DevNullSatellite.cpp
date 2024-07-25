/**
 * @file
 * @brief Implementation of random data sender satellites
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "DevNullSatellite.hpp"

#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation;

DevNullSatellite::DevNullSatellite(std::string_view type_name, std::string_view satellite_name)
    : Satellite(type_name, satellite_name) {}

void DevNullSatellite::receive(const message::CDTP1Message& msg) {
    LOG_IF(STATUS, msg.getHeader().getSequenceNumber() % 1000 == 0)
        << "snd " << msg.getHeader().getSender() << " seq " << msg.getHeader().getSequenceNumber();
}
