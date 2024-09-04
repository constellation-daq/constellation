#include "EudaqNativeWriterSatellite.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::satellite;

EudaqNativeWriterSatellite::FileSerializer::FileSerializer(
    const std::filesystem::path& path, std::string desc, std::uint32_t run_sequence, bool frames_as_blocks, bool overwrite)
    : file_(path, std::ios::binary), descriptor_(std::move(desc)), run_sequence_(run_sequence),
      frames_as_blocks_(frames_as_blocks) {
    if(std::filesystem::exists(path) && !overwrite) {
        throw SatelliteError("File path exists: " + path.string());
    }

    if(!file_.good()) {
        throw SatelliteError("Error opening file: " + path.string());
    }
}

EudaqNativeWriterSatellite::FileSerializer::~FileSerializer() {
    if(file_.is_open()) {
        file_.close();
    }
}

void EudaqNativeWriterSatellite::FileSerializer::write(const uint8_t* data, size_t len) {
    file_.write(reinterpret_cast<const char*>(data), len);
    if(!file_.good()) {
        throw SatelliteError("Error writing to file");
    }
    bytes_written_ += len;
}

void EudaqNativeWriterSatellite::FileSerializer::write_str(const std::string& t) {
    write_int(static_cast<std::uint32_t>(t.length()));
    write(reinterpret_cast<const uint8_t*>(&t[0]), t.length());
}

void EudaqNativeWriterSatellite::FileSerializer::write_tags(const Dictionary& dict) {
    LOG(DEBUG) << "Writing " << dict.size() << " event tags";

    write_int(static_cast<std::uint32_t>(dict.size()));
    for(auto i = dict.begin(); i != dict.end(); ++i) {
        write_str(i->first);
        write_str(i->second.str());
    }
}

void EudaqNativeWriterSatellite::FileSerializer::write_blocks(const std::vector<PayloadBuffer>& payload) {
    LOG(DEBUG) << "Writing " << payload.size() << " data blocks";

    // EUDAQ expects a map with frame number as key and vector of uint8_t as value:
    write_int(static_cast<std::uint32_t>(payload.size()));
    for(std::uint32_t key = 0; key < static_cast<uint32_t>(payload.size()); key++) {
        write_int(key);
        const auto frame = payload.at(key).span();
        write(reinterpret_cast<const std::uint8_t*>(frame.data()), frame.size_bytes());
    }
}

void EudaqNativeWriterSatellite::FileSerializer::serialize_bor_eor(const CDTP1Message::Header& header, Dictionary config) {

    const auto& tags = header.getTags();

    // Type, version and flags
    write_int(cstr2hash("RawEvent"));
    write_int(0u);
    write_int(0u);
    write_int(0u);

    // Run sequence
    write_int(run_sequence_);

    // Downcast event sequence for message header, use the same for trigger number
    write_int(static_cast<std::uint32_t>(header.getSequenceNumber()));
    write_int(static_cast<std::uint32_t>(header.getSequenceNumber()));

    // Writing ExtendWord (event description, used to identify decoder later on)
    write_int(cstr2hash(descriptor_.c_str()));

    // Timestamps from header tags if available - we get them in ps and write them in ns
    write_int(tags.contains("timestamp_begin") ? tags.at("timestamp_begin").get<std::uint64_t>() : 0ul);
    write_int(tags.contains("timestamp_end") ? tags.at("timestamp_end").get<std::uint64_t>() : 0ul);

    // Event description string
    write_str(descriptor_);

    // Write dictionary or metadata - event tags are ignored
    write_tags(config);

    // BOR does not contain data - write empty blocks and empty subevent count:
    write_blocks({});
    write_int(std::uint32_t());
}

void EudaqNativeWriterSatellite::FileSerializer::serialize(CDTP1Message&& data_message) {

    LOG(DEBUG) << "Writing data event";

    // Header
    const auto& header = data_message.getHeader();
    const auto& tags = header.getTags();

    // Payload

    // Type, version and flags
    write_int(cstr2hash("RawEvent"));
    write_int(0u);
    write_int(0u);

    // Number of devices/streams/planes - seems rarely used
    write_int(0u);

    // Run sequence
    write_int(run_sequence_);

    // Downcast event sequence for message header, use the same for trigger number
    write_int(static_cast<std::uint32_t>(header.getSequenceNumber()));
    write_int(static_cast<std::uint32_t>(header.getSequenceNumber()));

    // Writing ExtendWord (event description, used to identify decoder later on)
    write_int(cstr2hash(descriptor_.c_str()));

    // Timestamps from header tags if available - we get them in ps and write them in ns
    write_int(tags.contains("timestamp_begin") ? tags.at("timestamp_begin").get<std::uint64_t>() : 0ul);
    write_int(tags.contains("timestamp_end") ? tags.at("timestamp_end").get<std::uint64_t>() : 0ul);

    // Event description string
    write_str(descriptor_);

    // Header tags
    write_tags(tags);

    if(frames_as_blocks_) {
        // Interpret multiple frames as individual blocks of EUDAQ data:

        // Write block data:
        write_blocks(data_message.getPayload());

        // Zero sub-events:
        write_int(0u);
    } else {
        // Interpret multiple frames as EUDAQ sub-events:

        // FIXME
        // Write empty block data:
        // write(...);

        // Write sub-events:
        // write((uint32_t)m_sub_events.size());
        // for(auto &ev: m_sub_events){
        // write(*ev);
        // }
    }
}

void EudaqNativeWriterSatellite::FileSerializer::flush() {
    file_.flush();
}
