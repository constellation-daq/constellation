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
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/string_hash_map.hpp"
#include "constellation/core/utils/timers.hpp"
#include "constellation/satellite/ReceiverSatellite.hpp"

class EudaqNativeWriterSatellite final : public constellation::satellite::ReceiverSatellite {

    /** Event flags */
    enum class EUDAQFlags : std::uint32_t { // NOLINT(performance-enum-size)
        BORE = 0x1,
        EORE = 0x2,
        TRIGGER = 0x10,
    };

    /** Serializer class for EUDAQ native binary files */
    class FileSerializer {
    public:
        /**
         * @brief Constructor for file serializer
         * This attempts to open the file and checks if the path already exists
         *
         * @param path File path to open and to write to
         * @param run_sequence Sequence portion of the run identifier, used to store in EUDAQ event header
         * @param overwrite Boolean flag whether overwriting of files is allowed or not
         */
        FileSerializer(const std::filesystem::path& path, std::uint32_t run_sequence, bool overwrite = false);

        /**
         * @brief Destructor which flushes data to file and closes the file
         */
        virtual ~FileSerializer();

        /// @cond doxygen_suppress
        // No copy/move constructor/assignment
        FileSerializer(const FileSerializer& other) = delete;
        FileSerializer& operator=(const FileSerializer& other) = delete;
        FileSerializer(FileSerializer&& other) = delete;
        FileSerializer& operator=(FileSerializer&& other) = delete;
        /// @endcond

        /**
         * @brief Flush newly written content to disk
         */
        void flush();

        /**
         * @brief Function to serialize BOR and EOR messages which delimit a run
         * @details This function serializes the BOR and EOR delimiter CDTP messages into EUDAQ native binary format. The
         * corresponding EUDAQ events are marked as BORE and EORE, respectively. The header of the EUDAQ event will contain
         * the payload of the corresponding CDTP message, so the satellite configuration for the BOR and the run metadata for
         * the EOR, the header tags of the CDTP messages are not stored.
         *
         * Some file serializer settings are taken from the header tags for the BOR event to configure the further treatment
         * of the data.
         *
         * @param header [description]
         * @param payload [description]
         */
        void serializeDelimiterMsg(const constellation::message::CDTP1Message::Header& header,
                                   const constellation::config::Dictionary& payload);

        /**
         * @brief Function to serialize data messages
         * @details This function takes Constellation CDTP data messages and serializes them to the EUDAQ native binary
         * format. It first serializes header information of the event and then writes the data frames either as blocks
         * of the event or as sub-events, depending on the settings. For sub-events, the main header of the CDTP
         * message is repeated.
         *
         * @param data_message Constellation data message
         */
        void serializeDataMsg(const constellation::message::CDTP1Message& data_message);

    private:
        /**
         * @brief Function to serialize a EUDAQ event header
         *
         * @param header CDTP header
         * @param tags Tags to be serialized, either header tags of payload dictionaries
         * @param flags EUDAQ flags to mark the event with
         */
        void serialize_header(const constellation::message::CDTP1Message::Header& header,
                              const constellation::config::Dictionary& tags,
                              std::uint32_t flags = 0x0);

        /** Set eudaq event descriptors and frame handling from BOR tags */
        void parse_bor_tags(const constellation::message::CDTP1Message::Header& header);

        /** Write data to file */
        void write(std::span<const std::byte> data);

        /** Write EUDAQ event data blocks to file */
        void write_blocks(const std::vector<constellation::message::PayloadBuffer>& payload);

        /** Write a single EUDAQ event data block to file */
        void write_block(std::uint32_t key, const constellation::message::PayloadBuffer& payload);

        /** Write integers of different sizes to file */
        template <typename T> void write_int(T t) {
            static_assert(sizeof(t) > 1, "Only supports integers of size > 1 byte");
            std::array<std::byte, sizeof t> buf;
            for(std::size_t i = 0; i < sizeof t; ++i) {
                buf[i] = static_cast<std::byte>(t & 0xff);
                t >>= 8;
            }
            write({buf.data(), buf.size()});
        }

        /** Write a string to file */
        void write_str(const std::string& t);

        /** Write a dictionary to file */
        void write_tags(const constellation::config::Dictionary& dict);

        /** Helper function to hash a string into EUDAQ event identifiers */
        constexpr std::uint32_t cstr2hash(const char* str, std::uint32_t h = 0) { // NOLINT(misc-no-recursion)
            return !str[h] ? 5381 : (cstr2hash(str, h + 1) * 33ULL) ^ str[h];     // NOLINT
        }

    private:
        std::ofstream file_;
        std::uint32_t run_sequence_;

        constellation::utils::string_hash_map<std::string> eudaq_event_descriptors_;
        constellation::utils::string_hash_map<bool> frames_as_blocks_;
    };

public:
    /** Satellite constructor */
    EudaqNativeWriterSatellite(std::string_view type, std::string_view name);

    /** Transition function for initialize command */
    void initializing(constellation::config::Configuration& config) final;

    /** Transition function for start command */
    void starting(std::string_view run_identifier) final;

    /** Transition function for stop command */
    void stopping() final;

    /** Transition function for interrupt transition to SAFE mode */
    void interrupting(constellation::protocol::CSCP::State previous_state) final;

    /** Transition function for failure transition to ERROR mode */
    void failure(constellation::protocol::CSCP::State previous_state) final;

protected:
    /** Callback for receiving a BOR message */
    void receive_bor(const constellation::message::CDTP1Message::Header& header,
                     constellation::config::Configuration config) final;

    /** Callback for receiving a DATA message */
    void receive_data(constellation::message::CDTP1Message data_message) final;

    /** Callback for receiving a EOR message */
    void receive_eor(const constellation::message::CDTP1Message::Header& header,
                     constellation::config::Dictionary run_metadata) final;

private:
    std::unique_ptr<FileSerializer> serializer_;
    bool allow_overwriting_ {false};
    std::filesystem::path base_path_;
    constellation::utils::TimeoutTimer flush_timer_ {std::chrono::seconds(3)};
};
