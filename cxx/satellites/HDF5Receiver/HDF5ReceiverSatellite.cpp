/**
 * @file
 * @brief Implementation of data receiving and discarding satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "HDF5ReceiverSatellite.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <highfive/highfive.hpp>
#include <highfive/span.hpp>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/config/exceptions.hpp"
#include "constellation/core/config/Value.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/ReceiverSatellite.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::satellite;
using namespace constellation::utils;

namespace {
    std::string get_group_prefix(std::string_view sender) {
        return "/" + to_string(sender);
    }

    enum class AddDictAs : std::uint8_t {
        ATTRIBUTE,
        DATASET,
    };

    void add_dict_to(HighFive::Group& group, const Dictionary& dict, AddDictAs add_dict_as) {
        for(const auto& [key, value] : dict) {
            LOG(DEBUG) << "Adding key " << key << " with value " << value.str() << " as " << to_string(add_dict_as)
                       << " to group " << group.getPath();
            auto add_type_to = [&](auto&& value) {
                switch(add_dict_as) {
                case AddDictAs::ATTRIBUTE: {
                    group.createAttribute(key, value);
                    break;
                }
                case AddDictAs::DATASET: {
                    group.createDataSet(key, value);
                    break;
                }
                default: std::unreachable();
                }
            };
            std::visit(
                [&](auto&& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr(std::is_same_v<T, std::monostate>) {
                        add_type_to(std::vector<std::byte>());
                    } else {
                        add_type_to(value.get<T>());
                    }
                },
                value);
        }
    }

} // namespace

HDF5ReceiverSatellite::HDF5ReceiverSatellite(std::string_view type, std::string_view name) : ReceiverSatellite(type, name) {
    support_reconfigure();
}

void HDF5ReceiverSatellite::initializing(Configuration& config) {
    output_directory_ = config.getPath("output_directory", true);
    if(!std::filesystem::is_directory(output_directory_)) {
        throw InvalidValueError(output_directory_.string(), "output_directory", "needs to be a directory");
    }
    const auto flush_interval_s = config.get<std::int64_t>("flush_interval", 60);
    flush_timer_.setTimeout(std::chrono::seconds(flush_interval_s));
}

void HDF5ReceiverSatellite::starting(std::string_view run_identifier) {
    const auto file_path = output_directory_ / (to_string(run_identifier) + ".h5");
    hdf5_file_ = std::make_shared<HighFive::File>(file_path.string(), HighFive::File::Create);
    flush_timer_.reset();
}

void HDF5ReceiverSatellite::stopping() {
    // Close file by resetting ptr
    hdf5_file_->flush();
    hdf5_file_.reset();
    LOG(STATUS) << "Written data to file";
}

void HDF5ReceiverSatellite::receive_bor(const CDTP1Message::Header& header, Configuration config) {
    LOG(INFO) << "Received BOR from " << header.getSender() << " with config" << config.getDictionary().to_string();
    const auto bor_prefix = get_group_prefix(header.getSender()) + "/BOR";
    auto bor_group = hdf5_file_->createGroup(bor_prefix);
    add_dict_to(bor_group, header.getTags(), AddDictAs::ATTRIBUTE);
    add_dict_to(bor_group, config.getDictionary(), AddDictAs::DATASET);
}

void HDF5ReceiverSatellite::receive_data(
    CDTP1Message&& data_message) { // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
    const auto& header = data_message.getHeader();
    const auto message_prefix = get_group_prefix(header.getSender()) + "/DATA_" + to_string(header.getSequenceNumber());
    auto group = hdf5_file_->createGroup(message_prefix);
    add_dict_to(group, header.getTags(), AddDictAs::ATTRIBUTE);

    // Store frame as datasets in the group
    const auto frame_prefix = message_prefix + "/frame_";
    for(std::size_t frame_idx = 0; frame_idx < data_message.getPayload().size(); ++frame_idx) {
        hdf5_file_->createDataSet(frame_prefix + to_string(frame_idx), data_message.getPayload().at(frame_idx).span());
    }

    // If flush timer elapsed, flush file
    if(flush_timer_.timeoutReached()) [[unlikely]] {
        LOG(INFO) << "Flushing file";
        hdf5_file_->flush();
        flush_timer_.reset();
    }
}

void HDF5ReceiverSatellite::receive_eor(const CDTP1Message::Header& header, Dictionary run_metadata) {
    LOG(INFO) << "Received EOR from " << header.getSender() << " with metadata" << run_metadata.to_string();
    const auto eor_prefix = get_group_prefix(header.getSender()) + "/EOR";
    auto eor_group = hdf5_file_->createGroup(eor_prefix);
    add_dict_to(eor_group, header.getTags(), AddDictAs::ATTRIBUTE);
    add_dict_to(eor_group, run_metadata, AddDictAs::DATASET);
}
