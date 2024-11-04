/**
 * @file
 * @brief Dictionary type with serialization functions for MessagePack
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <map>
#include <string>
#include <vector>

#include <msgpack/object_decl.hpp>
#include <msgpack/pack_decl.hpp>
#include <msgpack/sbuffer_decl.hpp>

#include "constellation/build.hpp"
#include "constellation/core/config/Value.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"

namespace constellation::config {

    /**
     * List type with serialization functions for MessagePack
     */
    class List : public std::vector<Value> {
    public:
        /** Pack list with msgpack */
        CNSTLN_API void msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const;

        /** Unpack list with msgpack */
        CNSTLN_API void msgpack_unpack(const msgpack::object& msgpack_object);

        /** Assemble list via msgpack to message payload */
        CNSTLN_API message::PayloadBuffer assemble() const;

        /** Disassemble list from message payload */
        CNSTLN_API static List disassemble(const message::PayloadBuffer& message);

        /**
         * @brief Convert list to human readable string
         *
         * @return String with one line for each value starting `\n `
         */
        CNSTLN_API std::string to_string() const;
    };

    /**
     * Dictionary type with serialization functions for MessagePack and ZeroMQ
     */
    class Dictionary : public std::map<std::string, Value> {
    public:
        /** Pack dictionary with msgpack */
        CNSTLN_API void msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const;

        /** Unpack dictionary with msgpack */
        CNSTLN_API void msgpack_unpack(const msgpack::object& msgpack_object);

        /** Assemble dictionary via msgpack to message payload */
        CNSTLN_API message::PayloadBuffer assemble() const;

        /** Disassemble dictionary from message payload */
        CNSTLN_API static Dictionary disassemble(const message::PayloadBuffer& message);

        /**
         * @brief Convert dictionary to human readable string
         *
         * @return String with one line for each key-value pair starting `\n `
         */
        CNSTLN_API std::string to_string() const;
    };

} // namespace constellation::config
