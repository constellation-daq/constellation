/**
 * @file
 * @brief MsgPack helper functions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <exception>

#include <msgpack.hpp>

#include "constellation/core/utils/exceptions.hpp"

namespace constellation::utils {

    /**
     * @ingroup MsgPack Exceptions
     * @brief Error in encoding or decoding MsgPack data
     */
    class CNSTLN_API MsgPackError : public utils::RuntimeError {
    public:
        explicit MsgPackError(const std::string& type, const std::string& reason) {
            error_message_ = type;
            error_message_ += ": ";
            error_message_ += reason;
        }

    protected:
        MsgPackError() = default;
    };

    template <typename R, typename... Args> inline R msgpack_unpack_to(Args&&... args) try {
        const auto msgpack_var = msgpack::unpack(std::forward<Args>(args)...);
        return msgpack_var->template as<R>();
    } catch(const msgpack::type_error& e) {
        throw MsgPackError("Type error", e.what());
    } catch(const msgpack::parse_error& e) {
        throw MsgPackError("Error parsing data", e.what());
    } catch(const msgpack::unpack_error& e) {
        throw MsgPackError("Error unpacking data", e.what());
    };

} // namespace constellation::utils
