/**
 * @file
 * @brief Message Protocol Enum
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <magic_enum.hpp>

#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"

namespace constellation::message {

    /** Protocol Enum */
    enum class Protocol {
        /** Constellation Host Identification and Reconnaissance Protocol v2 */
        CHIRP2,
        /** Constellation Satellite Control Protocol v1 */
        CSCP1,
        /** Constellation Monitoring Distribution Protocol v1 */
        CMDP1,
        /** Constellation Data Transmission Protocol v1 */
        CDTP1,
        /** Constellation Heartbeat Protocol v1 */
        CHP1,
    };
    using enum Protocol;

    /**
     * @brief Get protocol identifier string
     *
     * @param protocol Protocol
     * @return Protocol identifier string in message header
     */
    std::string get_protocol_identifier(Protocol protocol) {
        // Convert to human readable string
        auto protocol_identifier = utils::to_string(protocol);
        // We know we only have 1-digit versions at the moment, so we can just convert the last digit
        // Digits in ASCII go from 48 (=0) to 57 (=9), so subtract 48 to get as int
        protocol_identifier.back() -= 48;
        return protocol_identifier;
    }

    /**
     * @brief Get protocol from a protocol identifier string
     *
     * @param protocol_identifier Protocol identifier string
     * @return Protocol
     * @throw std::invalid_argument If unknown protocol identifier
     */
    Protocol get_protocol(std::string protocol_identifier) {
        // We know we only have 1-digit versions at the moment, so we can just convert the last character
        // Digits in ASCII go from 48 (=0) to 57 (=9), so add 48 to get as digit
        protocol_identifier.back() += 48;
        // Try to cast to enum
        const auto protocol_opt = magic_enum::enum_cast<Protocol>(protocol_identifier);
        if(protocol_opt.has_value()) {
            return protocol_opt.value();
        }
        // Otherwise unknown protocol
        throw std::invalid_argument(protocol_identifier);
    }

} // namespace constellation::message
