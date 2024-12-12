/**
 * @file
 * @brief Network port definition
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdint>

namespace constellation::networking {

    /**
     * @brief Port number for a network connection
     *
     * Note that most ports in Constellation are ephemeral ports, meaning that the port numbers are allocated dynamically.
     * See also https://en.wikipedia.org/wiki/Ephemeral_port.
     */
    using Port = std::uint16_t;

} // namespace constellation::networking
