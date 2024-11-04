/**
 * @file
 * @brief Compatibility casts for std::byte
 *
 * Note: we should send MRs to msgpackc-cxx and cppzmq to support std::byte* instead
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstddef>
#include <ranges>
#include <span>

#include "constellation/core/utils/std_future.hpp"

namespace constellation::utils {

    template <typename T> inline const char* to_char_ptr(const T* data) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<const char*>(data);
    }

    template <typename T> inline char* to_char_ptr(T* data) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<char*>(data);
    }

    template <typename T> inline const void* to_void_ptr(const T* data) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<const void*>(data);
    }

    template <typename T> inline void* to_void_ptr(T* data) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<void*>(data);
    }

    template <typename T> inline const std::byte* to_byte_ptr(const T* data) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<const std::byte*>(data);
    }

    template <typename T> inline std::byte* to_byte_ptr(T* data) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<std::byte*>(data);
    }

    template <typename R>
        requires std::ranges::contiguous_range<R>
    inline std::span<const std::byte> to_byte_span(const R& range) {
        return std::as_bytes(std::span<std::ranges::range_value_t<R>>(std::ranges::cdata(range), std::ranges::size(range)));
    }

    template <typename R>
        requires std::ranges::contiguous_range<R> && (!std::ranges::constant_range<R>)
    inline std::span<std::byte> to_byte_span(R& range) {
        return std::as_writable_bytes(
            std::span<std::ranges::range_value_t<R>>(std::ranges::data(range), std::ranges::size(range)));
    }

} // namespace constellation::utils
