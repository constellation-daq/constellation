/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <functional>
#include <future>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>

#include "constellation/core/chirp/BroadcastSend.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/satellite/data/DataSender.hpp"
#include "constellation/satellite/data/SingleDataReceiver.hpp"

using namespace constellation;
using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::data;

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Sending / receiving data", "[satellite][satellite::data]") {
    auto chirp_manager = chirp::Manager("0.0.0.0", "0.0.0.0", "edda", "test");
    chirp_manager.setAsDefaultInstance();
    chirp_manager.start();
    auto config_sender = Configuration();
    auto config_receiver = Configuration();
    // Construct sender
    auto sender = DataSender("test");
    auto receiver = SingleDataReceiver();
    // Hack: add fake "test_mocked" satellite to chirp to find satellite (cannot find itself)
    chirp::BroadcastSend chirp_sender {"0.0.0.0", chirp::CHIRP_PORT};
    const auto chirp_msg = CHIRPMessage(chirp::MessageType::OFFER, "edda", "test_mocked", chirp::DATA, sender.getPort());
    chirp_sender.sendBroadcast(chirp_msg.assemble());
    // Initialize sender and receiver
    sender.initializing(config_sender);
    config_receiver.set("_data_sender_name", "test_mocked");
    receiver.initializing(config_receiver);
    // Launch receiver (find sender)
    receiver.launching();
    // Start sender and receiver
    auto starting_fut = std::async(std::launch::async, &DataSender::starting, &sender, std::ref(config_sender));
    const auto config_sender_received = receiver.starting();
    starting_fut.get();
    REQUIRE(config_sender_received.contains("_data_bor_timeout"));
    // Send data
    auto msg = sender.newDataMessage();
    msg.addDataFrame(std::vector<int>({1, 2, 3}));
    msg.addTag("count", 3);
    REQUIRE(sender.sendDataMessage(msg));
    // Receive data
    const auto msg_received_opt = receiver.recvData();
    REQUIRE(msg_received_opt.has_value());
    const auto& msg_received = msg_received_opt.value(); // NOLINT(bugprone-unchecked-optional-access)
    REQUIRE(msg_received.getHeader().getTag<int>("count") == 3);
    REQUIRE(msg_received.getPayload().size() == 1);
    // Stop sender and receiver
    receiver.stopping();
    auto run_metadata = Dictionary();
    run_metadata.emplace("metadata", "test");
    sender.setRunMetadata(std::move(run_metadata));
    auto stopping_fut = std::async(std::launch::async, &DataSender::stopping, &sender);
    REQUIRE_FALSE(receiver.recvData().has_value());
    stopping_fut.get();
    REQUIRE(receiver.gotEOR());
    const auto run_metadata_received = receiver.getEOR();
    REQUIRE(run_metadata_received.at("metadata").get<std::string>() == "test");
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
