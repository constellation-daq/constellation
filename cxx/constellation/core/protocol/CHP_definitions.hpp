/**
 * @file
 * @brief Additional definitions for the CHP protocol
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdint>

namespace constellation::protocol::CHP {

    /** Default lives for a remote on detection/replenishment */
    static constexpr std::uint8_t Lives = 3;

    /** Possible CHP message flags */
    enum MessageFlags : std::uint8_t {
        NONE = 0x00,

        /* Indicate a extrasystole message */
        IS_EXTRASYSTOLE = 0x01,

        /* Indicate whether the current state has been reached autonomously or by CSCP command */
        IS_AUTONOMOUS = 0x02,

        /* Indicate that this message contains a status text */
        HAS_STATUS = 0x04,
    };

} // namespace constellation::protocol::CHP
