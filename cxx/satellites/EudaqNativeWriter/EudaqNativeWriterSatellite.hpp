/**
 * @file
 * @brief Satellite receiving data and storing it as EUDAQ RawData files
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <filesystem>
#include <fstream>
#include <string_view>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/utils/timers.hpp"
#include "constellation/satellite/ReceiverSatellite.hpp"

class EudaqNativeWriterSatellite final : public constellation::satellite::ReceiverSatellite {

    class FileSerializer {
    public:
        FileSerializer(const std::filesystem::path& path,
                       std::string desc,
                       std::uint32_t run_sequence,
                       bool frames_as_blocks = true,
                       bool overwrite = false);
        void flush();
        uint64_t bytesWritten() const { return bytes_written_; }
        ~FileSerializer();

        void serialize(constellation::message::CDTP1Message&& data_message);

        void serialize_bor_eor(const constellation::message::CDTP1Message::Header& header,
                               constellation::config::Dictionary config);

    private:
        void write(const uint8_t* data, size_t len);

        void write_blocks(const std::vector<constellation::message::PayloadBuffer>& payload);

        template <typename T> void write_int(const T& v) {
            static_assert(sizeof(v) > 1, "Only supports integers of size > 1 byte");
            T t = v;
            uint8_t buf[sizeof v];
            for(size_t i = 0; i < sizeof v; ++i) {
                buf[i] = static_cast<uint8_t>(t & 0xff);
                t >>= 8;
            }
            write(buf, sizeof v);
        }

        void write_str(const std::string& t);

        void write_tags(const constellation::config::Dictionary& dict);

        constexpr uint32_t cstr2hash(const char* str, uint32_t h = 0) {
            return !str[h] ? 5381 : (cstr2hash(str, h + 1) * 33ULL) ^ str[h];
        }

        std::ofstream file_;
        std::uint64_t bytes_written_ {};
        std::string descriptor_;
        std::uint32_t run_sequence_;
        bool frames_as_blocks_;
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
    std::string descriptor_;
};
