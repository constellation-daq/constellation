/**
 * @file
 * @brief Payload buffer for message classes
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <any>
#include <concepts>
#include <cstddef>
#include <memory>
#include <ranges>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include <msgpack/sbuffer.hpp>
#include <zmq.hpp>

#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/std_future.hpp"

namespace constellation::message {

    /**
     * @brief Buffer holding an arbitrary object that can be turned into a `zmq::message_t`
     *
     * This buffer takes ownership of an arbitrary object that owns some memory. The buffer stores the object in an
     * `std::any` and memory range owned by the object in a span. The buffer features manual memory management to allow for
     * zero-copy message transport with ZeroMQ.
     *
     * To make the zero-copy mechanism work, the `std::any` is allocated via new on the heap. This allows to pass a free
     * function to ZeroMQ, which deallocates the `std::any` via delete. However, to avoid that the `std::any` is deleted when
     * the buffer goes out of scope, the buffer has to be released. This simply means that it resets the pointer to the
     * `std::any` such that it does not get deleted in the destructor of the buffer.
     *
     */
    class PayloadBuffer {
    public:
        /**
         * @brief Construct buffer given by moving an arbitrary object
         *
         * @note If T doesn't construct nicely to an `std::any`, one possibility is moving it into an `std::shared_ptr`
         *
         * @param t L-Value Reference of object T to move into the buffer
         * @param f Function F that takes a reference of T and returns an `std::span<std::byte>`
         */
        template <typename T, typename F>
            requires std::move_constructible<T> && std::is_invocable_r_v<std::span<std::byte>, F, T&>
        PayloadBuffer(T&& t, F f) : any_ptr_(new std::any(std::forward<T>(t))), span_(f(std::any_cast<T&>(*any_ptr_))) {}

        /**
         * @brief Specialized constructor for `zmq::message_t`
         */
        PayloadBuffer(zmq::message_t&& msg)
            : PayloadBuffer(std::make_shared<zmq::message_t>(std::move(msg)),
                            [](std::shared_ptr<zmq::message_t>& msg_ref) -> std::span<std::byte> {
                                return {utils::to_byte_ptr(msg_ref->data()), msg_ref->size()};
                            }) {}

        /**
         * @brief Specialized constructor for `msgpack::sbuffer`
         */
        PayloadBuffer(msgpack::sbuffer&& buf)
            : PayloadBuffer(std::make_shared<msgpack::sbuffer>(std::move(buf)),
                            [](std::shared_ptr<msgpack::sbuffer>& buf_ref) -> std::span<std::byte> {
                                return {utils::to_byte_ptr(buf_ref->data()), buf_ref->size()};
                            }) {}

        /**
         * @brief Specialized constructor for non-const ranges
         */
        template <typename R>
            requires std::ranges::contiguous_range<R> && (!std::ranges::constant_range<R>)
        PayloadBuffer(R&& range)
            : PayloadBuffer(std::forward<R>(range),
                            [](R& range_ref) -> std::span<std::byte> { return utils::to_byte_span(range_ref); }) {}

        /**
         * @brief Default constructor (empty payload)
         */
        PayloadBuffer() = default;

        /**
         * @brief Destructor deleting the `std::any`
         */
        ~PayloadBuffer() { delete any_ptr_; }

        // No copy constructor/assignment
        /// @cond doxygen_suppress
        PayloadBuffer(const PayloadBuffer& other) = delete;
        PayloadBuffer& operator=(const PayloadBuffer& other) = delete;
        /// @endcond

        /**
         * @brief Move constructor taking over other pointer and releasing other buffer
         */
        PayloadBuffer(PayloadBuffer&& other) noexcept : any_ptr_(other.any_ptr_), span_(other.span_) { other.release(); }

        /**
         * @brief Move assignment freeing the buffer, taking over other pointer and releasing other buffer
         */
        PayloadBuffer& operator=(PayloadBuffer&& other) noexcept {
            // Free any memory still owned
            delete any_ptr_;
            // Copy pointer from other buffer
            any_ptr_ = other.any_ptr_;
            span_ = other.span_;
            // Release other buffer to transfer ownership
            other.release();

            return *this;
        }

        /**
         * @brief Read-only access to the data in the buffer
         *
         * @return Constant span of the data
         */
        constexpr std::span<const std::byte> span() const { return span_; }

        /**
         * @brief Write access to the data in the buffer
         *
         * @return Non-constant span of the data
         */
        constexpr std::span<std::byte> span() { return span_; }

        /**
         * @brief Check if the payload is empty
         *
         * @return If the span is empty
         */
        constexpr bool empty() const { return span_.empty(); }

        /**
         * @brief Interpret data as string
         *
         * @note This function does not ensure that the data in the buffer is actually a string
         *
         * @return String of the content in the buffer
         */
        std::string_view to_string_view() const { return {utils::to_char_ptr(span_.data()), span_.size()}; }

        /**
         * @brief Create a ZeroMQ message by copying the buffer
         *
         * @return ZeroMQ message with a copy of the data in the buffer
         */
        zmq::message_t to_zmq_msg_copy() const {
            // zmq::message_t constructor will copy the data into a new buffer
            return {span_.data(), span_.size()};
        }

        /**
         * @brief Create a ZeroMQ message for zero-copy transport
         *
         * @note The buffer will be released after this function, meaning that the buffer cannot be used anymore
         *
         * @return ZeroMQ message owning the data in the buffer
         */
        zmq::message_t to_zmq_msg_release() {
            // Free function for zero-copy: delete std::any owning the memory
            auto free_fn = [](void* /* data */, void* hint) {
                auto* any_ptr = reinterpret_cast<std::any*>(hint); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                delete any_ptr;                                    // NOLINT(cppcoreguidelines-owning-memory)
            };
            // zmq::message_t constructor with free function does not copy the data, pass any_ptr_ as hint
            auto msg = zmq::message_t(span_.data(), span_.size(), free_fn, any_ptr_);
            // Release buffer since ZeroMQ took over ownership of the data
            release();

            return msg;
        }

    private:
        // Release buffer: reset pointer and span without deleting the memory-owning object
        void release() noexcept {
            any_ptr_ = nullptr;
            span_ = {};
        }

    private:
        std::any* any_ptr_ {nullptr};
        std::span<std::byte> span_;
    };

} // namespace constellation::message
