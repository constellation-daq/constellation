/**
 * @file
 * @brief Message Protocol Enum
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"

namespace constellation::protocol {

    /** Protocol Enum (excluding CHIRP) */
    enum class Protocol : std::uint8_t {
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
     * Get protocol identifier string for CSCP, CMDP and CDTP protocols
     *
     * @param protocol Protocol
     * @return Protocol identifier string in message header
     */
    constexpr std::string_view get_protocol_identifier(Protocol protocol) {
        switch(protocol) {
        case CSCP1: return {"CSCP\x01"};
        case CMDP1: return {"CMDP\x01"};
        case CDTP1: return {"CDTP\x01"};
        case CHP1: return {"CHP\x01"};
        default: std::unreachable();
        }
    }

    /**
     * Get protocol from a protocol identifier string
     *
     * @param protocol_identifier Protocol identifier string
     * @return Protocol
     */
    constexpr Protocol get_protocol(std::string_view protocol_identifier) {
        if(protocol_identifier == "CSCP\x01") {
            return CSCP1;
        }
        if(protocol_identifier == "CMDP\x01") {
            return CMDP1;
        }
        if(protocol_identifier == "CDTP\x01") {
            return CDTP1;
        }
        if(protocol_identifier == "CHP\x01") {
            return CHP1;
        }
        // Unknown protocol:
        throw std::invalid_argument(std::string(protocol_identifier).c_str());
    }

    /**
     * Get human-readable protocol identifier string for CSCP, CMDP and CDTP protocols
     *
     * @param protocol_identifier Protocol identifier string
     * @return Protocol identifier string with byte version replaced to human-readable version
     */
    inline std::string get_readable_protocol(std::string_view protocol_identifier) {
        std::string out {protocol_identifier.data(), protocol_identifier.size() - 1};
        // TODO(stephan.lachnit): make this general by finding all non-ASCII symbols and convert them to numbers
        out += utils::to_string(protocol_identifier.back());
        return out;
    }

    /**
     * Get human-readable protocol identifier string for CSCP, CMDP and CDTP protocols
     *
     * @param protocol Protocol
     * @return Protocol identifier string with byte version replaced to human-readable version
     */
    inline std::string get_readable_protocol(Protocol protocol) {
        auto protocol_identifier = get_protocol_identifier(protocol);
        return get_readable_protocol(protocol_identifier);
    }

} // namespace constellation::protocol
