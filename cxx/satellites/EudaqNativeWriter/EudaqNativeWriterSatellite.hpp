/**
 * @file
 * @brief Satellite receiving data and storing it as EUDAQ RawData files
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/satellite/ReceiverSatellite.hpp"

class EudaqNativeWriterSatellite final : public constellation::satellite::ReceiverSatellite {

    /** Event flags */
    enum class EUDAQFlags : std::uint32_t { // NOLINT(performance-enum-size)
        BORE = 0x1,
        EORE = 0x2,
    };

    class FileSerializer {
    public:
        FileSerializer(const std::filesystem::path& path, std::uint32_t run_sequence, bool overwrite = false);
        virtual ~FileSerializer();

        /// @cond doxygen_suppress
        // No copy/move constructor/assignment
        FileSerializer(const FileSerializer& other) = delete;
        FileSerializer& operator=(const FileSerializer& other) = delete;
        FileSerializer(FileSerializer&& other) = delete;
        FileSerializer& operator=(FileSerializer&& other) = delete;
        /// @endcond

        void flush();
        std::uint64_t bytesWritten() const { return bytes_written_; }

        void serializeDataMsg(const constellation::message::CDTP1Message& data_message);

        void serializeDelimiterMsg(const constellation::message::CDTP1Message::Header& header,
                                   const constellation::config::Dictionary& config);

    private:
        void serialize_header(const constellation::message::CDTP1Message::Header& header,
                              const constellation::config::Dictionary& tags,
                              std::uint32_t flags = 0x0);

        void write(const std::uint8_t* data, std::size_t len);

        void write_blocks(const std::vector<constellation::message::PayloadBuffer>& payload);
        void write_block(std::uint32_t key, const constellation::message::PayloadBuffer& payload);

        template <typename T> void write_int(const T& v) {
            static_assert(sizeof(v) > 1, "Only supports integers of size > 1 byte");
            T t = v;
            std::array<uint8_t, sizeof v> buf;
            for(std::size_t i = 0; i < sizeof v; ++i) {
                buf[i] = static_cast<std::uint8_t>(t & 0xff);
                t >>= 8;
            }
            write(buf.data(), buf.size());
        }

        void write_str(const std::string& t);

        void write_tags(const constellation::config::Dictionary& dict);

        constexpr std::uint32_t cstr2hash(const char* str, std::uint32_t h = 0) { // NOLINT(misc-no-recursion)
            return !str[h] ? 5381 : (cstr2hash(str, h + 1) * 33ULL) ^ str[h];     // NOLINT
        }

    private:
        std::ofstream file_;
        std::uint64_t bytes_written_ {};
        std::uint32_t run_sequence_;

        std::map<std::string, std::string> eudaq_event_descriptors_;
        std::map<std::string, bool> frames_as_blocks_;
    };

public:
    EudaqNativeWriterSatellite(std::string_view type, std::string_view name);

    void initializing(constellation::config::Configuration& config) final;
    void starting(std::string_view run_identifier) final;
    void stopping() final;

protected:
    void receive_bor(const constellation::message::CDTP1Message::Header& header,
                     constellation::config::Configuration config) final;
    void receive_data(constellation::message::CDTP1Message&& data_message) final;
    void receive_eor(const constellation::message::CDTP1Message::Header& header,
                     constellation::config::Dictionary run_metadata) final;

private:
    std::unique_ptr<FileSerializer> serializer_;
};
