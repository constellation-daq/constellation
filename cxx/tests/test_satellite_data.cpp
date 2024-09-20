/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "constellation/core/chirp/BroadcastSend.hpp"
#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/ReceiverSatellite.hpp"
#include "constellation/satellite/TransmitterSatellite.hpp"

#include "dummy_satellite.hpp"

using namespace Catch::Matchers;
using namespace constellation;
using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::satellite;
using namespace constellation::utils;

class Receiver : public DummySatelliteNR<ReceiverSatellite> {
protected:
    void receive_bor(const CDTP1Message::Header& header, Configuration config) override {
        const auto sender = to_string(header.getSender());
        const std::lock_guard map_lock {map_mutex_};
        bor_map_.erase(sender);
        bor_map_.emplace(sender, std::move(config));
    }
    void receive_data(CDTP1Message&& data_message) override {
        const auto sender = to_string(data_message.getHeader().getSender());
        const std::lock_guard map_lock {map_mutex_};
        last_data_map_.erase(sender);
        last_data_map_.emplace(sender, std::move(data_message));
    }
    void receive_eor(const CDTP1Message::Header& header, Dictionary run_metadata) override {
        const auto sender = to_string(header.getSender());
        const std::lock_guard map_lock {map_mutex_};
        eor_map_.erase(sender);
        eor_map_.emplace(sender, std::move(run_metadata));
    }

public:
    const Configuration& getBOR(const std::string& sender) {
        const std::lock_guard map_lock {map_mutex_};
        return bor_map_.at(sender);
    }
    const CDTP1Message& getLastData(const std::string& sender) {
        const std::lock_guard map_lock {map_mutex_};
        return last_data_map_.at(sender);
    }
    const Dictionary& getEOR(const std::string& sender) {
        const std::lock_guard map_lock {map_mutex_};
        return eor_map_.at(sender);
    }

private:
    std::mutex map_mutex_;
    std::map<std::string, Configuration> bor_map_;
    std::map<std::string, CDTP1Message> last_data_map_;
    std::map<std::string, Dictionary> eor_map_;
};

class Transmitter : public DummySatellite<TransmitterSatellite> {
public:
    Transmitter(std::string_view name = "t1") : DummySatellite<TransmitterSatellite>(name) {}

    template <typename T> bool sendData(T data) {
        auto msg = newDataMessage();
        msg.addFrame(std::move(data));
        msg.addTag("test", 1);
        return sendDataMessage(msg);
    }

    template <typename T> void trySendData(T data) {
        auto msg = newDataMessage();
        msg.addFrame(std::move(data));
        msg.addTag("test", 1);
        trySendDataMessage(msg);
    }
};

void chirp_mock_service(std::string_view name, Port port) {
    // Hack: add fake satellite to chirp to find satellite (cannot find from same manager)
    chirp::BroadcastSend chirp_sender {"0.0.0.0", chirp::CHIRP_PORT};
    const auto chirp_msg = CHIRPMessage(chirp::MessageType::OFFER, "edda", name, chirp::DATA, port);
    chirp_sender.sendBroadcast(chirp_msg.assemble());
}

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Receiver / No transmitters configured", "[satellite]") {
    auto receiver = Receiver();
    auto config = Configuration();
    config.set("_eor_timeout", 1);
    receiver.reactFSM(FSM::Transition::initialize, std::move(config));
    // Require receiver in error state because _data_transmitters missing
    REQUIRE(receiver.getState() == FSM::State::ERROR);
}

TEST_CASE("Transmitter / BOR timeout", "[satellite]") {
    auto transmitter = Transmitter();
    auto config = Configuration();
    config.set("_bor_timeout", 1);
    transmitter.reactFSM(FSM::Transition::initialize, std::move(config));
    transmitter.reactFSM(FSM::Transition::launch);
    transmitter.reactFSM(FSM::Transition::start, "test");
    // Require that transmitter went to error state
    REQUIRE(transmitter.getState() == FSM::State::ERROR);
}

TEST_CASE("Transmitter / DATA timeout", "[satellite]") {
    // Create CHIRP manager for data service discovery
    auto chirp_manager = chirp::Manager("0.0.0.0", "0.0.0.0", "edda", "test");
    chirp_manager.setAsDefaultInstance();
    chirp_manager.start();

    auto transmitter = Transmitter();
    chirp_mock_service("Dummy.t1", transmitter.getDataPort());

    auto receiver = Receiver();
    auto config_receiver = Configuration();
    config_receiver.set("_eor_timeout", 1);
    config_receiver.setArray<std::string>("_data_transmitters", {"Dummy.t1"});

    auto config_transmitter = Configuration();
    config_transmitter.set("_data_timeout", 1);

    receiver.reactFSM(FSM::Transition::initialize, std::move(config_receiver));
    transmitter.reactFSM(FSM::Transition::initialize, std::move(config_transmitter));
    receiver.reactFSM(FSM::Transition::launch);
    transmitter.reactFSM(FSM::Transition::launch);
    receiver.reactFSM(FSM::Transition::start, "test");
    transmitter.reactFSM(FSM::Transition::start, "test");

    // Wait a bit for BOR to be handled by receiver
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    REQUIRE(receiver.getBOR("Dummy.t1").get<int>("_bor_timeout") == 10);

    // Stop the receiver to avoid receiving data
    receiver.reactFSM(FSM::Transition::stop);

    // Attempt to send a data frame and catch its failure
    REQUIRE_THROWS_MATCHES(transmitter.trySendData(std::vector<int>({1, 2, 3, 4})),
                           SendTimeoutError,
                           Message("Failed sending data message after 1s"));
}

TEST_CASE("Successful run", "[satellite]") {
    // Create CHIRP manager for data service discovery
    auto chirp_manager = chirp::Manager("0.0.0.0", "0.0.0.0", "edda", "test");
    chirp_manager.setAsDefaultInstance();
    chirp_manager.start();

    auto receiver = Receiver();
    auto transmitter = Transmitter();
    chirp_mock_service("Dummy.t1", transmitter.getDataPort());

    auto config_receiver = Configuration();
    config_receiver.setArray<std::string>("_data_transmitters", {"Dummy.t1"});

    receiver.reactFSM(FSM::Transition::initialize, std::move(config_receiver));
    transmitter.reactFSM(FSM::Transition::initialize, Configuration());
    receiver.reactFSM(FSM::Transition::launch);
    transmitter.reactFSM(FSM::Transition::launch);

    auto config2_receiver = Configuration();
    config2_receiver.set("_eor_timeout", 1);
    config2_receiver.setArray<std::string>("_data_transmitters", {"Dummy.t1", "Dummy.t2"});
    auto config2_transmitter = Configuration();
    config2_transmitter.set("_bor_timeout", 1);
    config2_transmitter.set("_eor_timeout", 1);
    receiver.reactFSM(FSM::Transition::reconfigure, std::move(config2_receiver));
    transmitter.reactFSM(FSM::Transition::reconfigure, std::move(config2_transmitter));

    receiver.reactFSM(FSM::Transition::start, "test");
    transmitter.reactFSM(FSM::Transition::start, "test");

    // Wait a bit for BOR to be handled by receiver
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    REQUIRE(receiver.getBOR("Dummy.t1").get<int>("_bor_timeout") == 1);

    // Send a data frame
    const auto sent = transmitter.sendData(std::vector<int>({1, 2, 3, 4}));
    REQUIRE(sent);
    // Wait a bit for data to be handled by receiver
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto& data_msg = receiver.getLastData("Dummy.t1");
    REQUIRE(data_msg.countPayloadFrames() == 1);
    REQUIRE(data_msg.getHeader().getTag<int>("test") == 1);

    // Set a run metadata tag for EOR
    transmitter.setRunMetadataTag("buggy_events", 10);

    // Stop and send EOR
    receiver.reactFSM(FSM::Transition::stop, {}, false);
    transmitter.reactFSM(FSM::Transition::stop);
    // Wait until EOR is handled
    receiver.progressFsm();
    const auto& eor = receiver.getEOR("Dummy.t1");
    REQUIRE(eor.at("run_id").get<std::string>() == "test");
    REQUIRE(eor.at("buggy_events").get<int>() == 10);

    // Ensure all satellite are happy
    REQUIRE(receiver.getState() == FSM::State::ORBIT);
    REQUIRE(transmitter.getState() == FSM::State::ORBIT);
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
