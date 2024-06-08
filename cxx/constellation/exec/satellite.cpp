/**
 * @file
 * @brief Implementation of the main function for a satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "satellite.hpp"

#include <cctype>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <argparse/argparse.hpp>
#include <asio.hpp>
#include <magic_enum.hpp>

#include "constellation/build.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/logging/Level.hpp"
#include "constellation/core/logging/log.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/core/logging/SinkManager.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/exec/DSOLoader.hpp"
#include "constellation/exec/exceptions.hpp"
#include "constellation/satellite/Satellite.hpp"
#include "constellation/satellite/SatelliteImplementation.hpp"

using namespace constellation;
using namespace constellation::exec;
using namespace constellation::log;
using namespace constellation::satellite;
using namespace constellation::utils;

// Use global std::function to work around C linkage
std::function<void(int)> signal_handler_f {}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

extern "C" void signal_hander(int signal) {
    signal_handler_f(signal);
}

// NOLINTNEXTLINE(*-avoid-c-arrays)
void parse_args(int argc, char* argv[], argparse::ArgumentParser& parser, bool needs_type) {
    // If not a predefined type, requires that the satellite type is specified
    if(needs_type) {
        parser.add_argument("-t", "--type").help("satellite type").required();
    }

    // Satellite name (-n)
    // Note: canonical satellite name = type_name.satellite_name
    try {
        // Ty to use host name as default
        parser.add_argument("-n", "--name").help("satellite name").default_value(asio::ip::host_name());
    } catch(const asio::system_error& error) {
        parser.add_argument("-n", "--name").help("satellite name").required();
    }

    // Constellation group (-g)
    parser.add_argument("-g", "--group").help("group name").required();

    // Console log level (-l)
    parser.add_argument("-l", "--level").help("log level").default_value("INFO");

    // TODO(stephan.lachnit): module specific console log level

    // Broadcast address (--brd)
    std::string default_brd_addr {};
    try {
        default_brd_addr = asio::ip::address_v4::broadcast().to_string();
    } catch(const asio::system_error& error) {
        default_brd_addr = "255.255.255.255";
    }
    parser.add_argument("--brd").help("broadcast address").default_value(default_brd_addr);

    // Any address (--any)
    std::string default_any_addr {};
    try {
        default_any_addr = asio::ip::address_v4::any().to_string();
    } catch(const asio::system_error& error) {
        default_any_addr = "0.0.0.0";
    }
    parser.add_argument("--any").help("any address").default_value(default_any_addr);

    // Note: this might throw
    parser.parse_args(argc, argv);
}

// parser.get() might throw a logic error, but this never happens in practice
std::string get_arg(argparse::ArgumentParser& parser, std::string_view arg) noexcept {
    try {
        return parser.get(arg);
    } catch(const std::exception&) {
        std::unreachable();
    }
}

int constellation::exec::satellite_main(int argc,
                                        char* argv[], // NOLINT(*-avoid-c-arrays)
                                        std::string_view program,
                                        std::optional<SatelliteType> satellite_type) noexcept {
    // Ensure that ZeroMQ doesn't fail creating the CMDP sink
    try {
        SinkManager::getInstance().enableCMDPBacktrace();
    } catch(const ZMQInitError& error) {
        std::cerr << "Failed to initialize logging: " << error.what() << std::endl;
        return 1;
    }

    // Get the default logger
    auto& logger = Logger::getDefault();

    // If we need to parse the type name via CLI
    const auto needs_type = !satellite_type.has_value();

    // CLI parsing
    argparse::ArgumentParser parser {to_string(program), CNSTLN_VERSION};
    try {
        parse_args(argc, argv, parser, needs_type);
    } catch(const std::exception& error) {
        LOG(logger, CRITICAL) << "Argument parsing failed: " << error.what();
        LOG(logger, CRITICAL) << "Run \"" << program << " --help\" for help";
        return 1;
    }

    // Set log level
    const auto default_level = magic_enum::enum_cast<Level>(get_arg(parser, "level"), magic_enum::case_insensitive);
    if(!default_level.has_value()) {
        LOG(logger, CRITICAL) << "Log level \"" << get_arg(parser, "level") << "\" is not valid"
                              << ", possible values are: " << utils::list_enum_names<Level>();
        return 1;
    }
    SinkManager::getInstance().setGlobalConsoleLevel(default_level.value());

    // Check broadcast and any address
    asio::ip::address brd_addr {};
    try {
        brd_addr = asio::ip::address::from_string(get_arg(parser, "brd"));
    } catch(const asio::system_error& error) {
        LOG(logger, CRITICAL) << "Invalid broadcast address \"" << get_arg(parser, "brd") << "\"";
        return 1;
    }
    asio::ip::address any_addr {};
    try {
        any_addr = asio::ip::address::from_string(get_arg(parser, "any"));
    } catch(const asio::system_error& error) {
        LOG(logger, CRITICAL) << "Invalid any address \"" << get_arg(parser, "any") << "\"";
        return 1;
    }

    // Check satellite name
    const auto type_name = needs_type ? get_arg(parser, "type") : std::move(satellite_type.value().type_name);
    const auto satellite_name = get_arg(parser, "name");
    const auto canonical_name = type_name + "." + satellite_name;
    // TODO(stephan.lachnit): check if names are valid

    // Log the version after all the basic checks are done
    LOG(logger, STATUS) << "Constellation v" << CNSTLN_VERSION;

    // Create CHIRP manager and set as default
    std::unique_ptr<chirp::Manager> chirp_manager {};
    try {
        chirp_manager = std::make_unique<chirp::Manager>(brd_addr, any_addr, parser.get("group"), canonical_name);
        chirp_manager->setAsDefaultInstance();
        chirp_manager->start();
    } catch(const std::exception& error) {
        LOG(logger, CRITICAL) << "Failed to initiate network discovery: " << error.what();
        // TODO(stephan.lachnit): should we continue anyway or abort?
    }

    // Start sending CMDP messages
    SinkManager::getInstance().enableCMDPSending(canonical_name);

    // Load satellite DSO
    std::unique_ptr<DSOLoader> loader {};
    Generator* satellite_generator {};
    try {
        loader = needs_type ? std::make_unique<DSOLoader>(type_name, logger)
                            : std::make_unique<DSOLoader>(type_name, logger, satellite_type.value().dso_path);
        satellite_generator = loader->loadSatelliteGenerator();
    } catch(const DSOLoaderError& error) {
        LOG(logger, CRITICAL) << "Error loading satellite type \"" << type_name << "\": " << error.what();
        return 1;
    }

    // Create satellite
    LOG(logger, STATUS) << "Starting satellite " << canonical_name;
    std::shared_ptr<Satellite> satellite {};
    try {
        satellite = satellite_generator(type_name, satellite_name);
    } catch(const std::exception& error) {
        LOG(logger, CRITICAL) << "Failed to create satellite: " << error.what();
        return 1;
    }

    // Start satellite
    SatelliteImplementation satellite_implementation {std::move(satellite)};
    satellite_implementation.start();

    // Register signal handlers
    std::once_flag shut_down_flag {};
    signal_handler_f = [&](int /*signal*/) -> void {
        std::call_once(shut_down_flag, [&]() {
            LOG(logger, STATUS) << "Terminating satellite";
            satellite_implementation.terminate();
        });
    };
    // NOLINTBEGIN(cert-err33-c)
    std::signal(SIGTERM, &signal_hander);
    std::signal(SIGINT, &signal_hander);
    // NOLINTEND(cert-err33-c)

    // Wait for signal to join
    satellite_implementation.join();

    return 0;
}
