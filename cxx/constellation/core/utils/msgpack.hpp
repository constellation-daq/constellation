/**
 * @file
 * @brief MsgPack helper functions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <msgpack.hpp>

#include "constellation/core/utils/exceptions.hpp"

namespace constellation::utils {

    /**
     * @brief MsgPack helper function to pack value to stream
     * @details This helper catches all MsgPack exceptions and rethrows them as MsgpackPackError
     *
     * @tparam S Reference to target stream
     * @tparam T Value to be packed
     */
    template <typename S, typename T> inline void msgpack_pack(S& stream, const T& object) try {
        msgpack::pack(stream, object);
    } catch(const msgpack::type_error& e) {
        throw MsgpackPackError("Type error", e.what());
    } catch(const msgpack::parse_error& e) {
        throw MsgpackPackError("Error parsing data", e.what());
    };

    /**
     * @brief MsgPack helper to unpack value
     * @details This helper unpacks the MsgPack value and returns it as target value type. It catches all MsgPack exceptions
     *          and rethrows them as MsgpackUnpackError
     *
     * @tparam args Variadic arguments
     * @return Unpacked value in target type
     */
    template <typename R, typename... Args> inline R msgpack_unpack_to(Args&&... args) try {
        const auto msgpack_var = msgpack::unpack(std::forward<Args>(args)...);
        return msgpack_var->template as<R>();
    } catch(const msgpack::type_error& e) {
        throw MsgpackUnpackError("Type error", e.what());
    } catch(const msgpack::parse_error& e) {
        throw MsgpackUnpackError("Error parsing data", e.what());
    } catch(const msgpack::unpack_error& e) {
        throw MsgpackUnpackError("Error unpacking data", e.what());
    };

} // namespace constellation::utils
