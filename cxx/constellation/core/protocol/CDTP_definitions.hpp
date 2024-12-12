/**
 * @file
 * @brief Additional definitions for the CDTP protocol
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdint>

namespace constellation::protocol::CDTP {

    /** Possible conditions of a run */
    enum RunCondition : std::uint8_t {
        /** The run has concluded normally, no other information has been provided by the sender */
        GOOD = 0x00,

        /** The data has been marked as tainted by the sender */
        TAINTED = 0x01,

        /** The receiver has noticed missing messages in the sequence */
        INCOMPLETE = 0x02,

        /** The run has been interrupted by this sender because of a failure condition elsewhere in the constellation */
        INTERRUPTED = 0x04,

        /** The run has been aborted by the sender and the EOR message has been appended by the receiver */
        ABORTED = 0x08,
    };

} // namespace constellation::protocol::CDTP
