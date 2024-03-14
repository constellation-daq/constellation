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
#include <exception>
#include <string>

#include <argparse/argparse.hpp>
#include <asio.hpp>
#include <magic_enum.hpp>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/config.hpp"
#include "constellation/core/logging/log.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/core/logging/SinkManager.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/exec/DSOLoader.hpp"
#include "constellation/exec/exceptions.hpp"
#include "constellation/satellite/Satellite.hpp"
#include "constellation/satellite/SatelliteImplementation.hpp"

using namespace constellation;
using namespace constellation::exec;
using namespace constellation::log;
using namespace constellation::satellite;

void parse_args(int argc, char* argv[], argparse::ArgumentParser& parser, bool needs_class) {
    // If not a predefined class, requires that the satellite class is specified
    if(needs_class) {
        parser.add_argument("-c", "--class").help("satellite class").required();
    }

    // Satellite device name (-n)
    // Note: full satellite name = class.device_name
    try {
        auto default_device_name = asio::ip::host_name();
        parser.add_argument("-n", "--name").help("device name").default_value(std::move(default_device_name));
    } catch(const asio::system_error& error) {
        parser.add_argument("-n", "--name").help("device name").required();
    }

    // Constellation group (-g)
    parser.add_argument("-g", "--group").help("constellation group name").required();

    // Console log level (-l)
    parser.add_argument("-l", "--level").help("log level").default_value("INFO");

    // TODO(stephan.lachnit): module specific console log level

    // Broadcast address (--brd)
    std::string default_brd_addr {};
    try {
        default_brd_addr = asio::ip::address_v4::broadcast().to_string();
    } catch(const asio::system_error& error) {
        default_brd_addr = "255.255.255.255";
        // Use 255.255.255.255 as default
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

int constellation::exec::satellite_main(int argc,
                                        char* argv[],
                                        std::string program,
                                        std::optional<SatelliteClass> satellite_class) noexcept {
    // root logger
    auto logger = Logger("ROOT");

    // If we need to parse the class name via CLI
    const auto needs_class = !satellite_class.has_value();

    // CLI parsing
    argparse::ArgumentParser parser {program, CNSTLN_VERSION};
    try {
        parse_args(argc, argv, parser, needs_class);
    } catch(const std::exception& error) {
        LOG(logger, CRITICAL) << "Argument parsing failed: " << error.what();
        LOG(logger, CRITICAL) << "Run \"" << program << " --help\" for help";
        return 1;
    }

    // Set log level
    const auto default_level_str = utils::transform(parser.get("level"), ::toupper);
    const auto default_level = magic_enum::enum_cast<Level>(default_level_str);
    if(!default_level.has_value()) {
        LOG(logger, CRITICAL) << "Log level \"" << default_level_str << "\" is not valid, "
                              << "please choose from TRACE, DEBUG, INFO, STATUS, WARNING, CRITICAL, OFF";
        return 1;
    }
    SinkManager::getInstance().setGlobalConsoleLevel(default_level.value());

    // Check broadcast and any address
    asio::ip::address brd_addr {};
    try {
        brd_addr = asio::ip::address::from_string(parser.get("brd"));
    } catch(const asio::system_error& error) {
        LOG(logger, CRITICAL) << "Invalid broadcast address \"" << parser.get("brd") << "\"";
        return 1;
    }
    asio::ip::address any_addr {};
    try {
        any_addr = asio::ip::address::from_string(parser.get("any"));
    } catch(const asio::system_error& error) {
        LOG(logger, CRITICAL) << "Invalid any address \"" << parser.get("any") << "\"";
        return 1;
    }

    // Check satellite name
    const auto class_str = needs_class ? parser.get("class") : satellite_class.value().class_name;
    const auto device_name_str = parser.get("name");
    std::string satellite_name = class_str + "." + device_name_str;
    // TODO(stephan.lachnit): check if name is valid

    // Log the version after all the basic checks are done
    LOG(logger, STATUS) << "Constellation v" << CNSTLN_VERSION;

    // Load satellite class DSO
    std::unique_ptr<DSOLoader> loader {};
    Generator* satellite_generator {};
    try {
        loader = needs_class ? std::make_unique<DSOLoader>(class_str, logger)
                             : std::make_unique<DSOLoader>(class_str, logger, satellite_class.value().dso_path);
        satellite_generator = loader->loadSatelliteGenerator();
    } catch(const DSOLoaderError& error) {
        LOG(logger, CRITICAL) << "Error loading satellite class \"" << class_str << "\": " << error.what();
        return 1;
    }

    // Create satellite
    LOG(logger, STATUS) << "Starting satellite " << satellite_name;
    std::shared_ptr<Satellite> satellite {};
    try {
        satellite = satellite_generator(satellite_name);
    } catch(const std::exception& error) {
        LOG(logger, CRITICAL) << "Failed to create satellite: " << error.what();
        return 1;
    }

    // Create CHIRP manager and set as default
    std::unique_ptr<chirp::Manager> chirp_manager {};
    try {
        chirp_manager = std::make_unique<chirp::Manager>(brd_addr, any_addr, parser.get("group"), satellite_name);
        chirp_manager->setAsDefaultInstance();
        chirp_manager->start();
    } catch(const std::exception& error) {
        LOG(logger, CRITICAL) << "Failed to initiate network discovery: " << error.what();
        // TODO(stephan.lachnit): should we continue anyway or abort?
    }

    // Register CMDP in CHIRP
    SinkManager::getInstance().registerService();

    // Start satellite
    SatelliteImplementation satellite_implementation {satellite_name, satellite};
    satellite_implementation.start();

    // TODO(stephan.lachnit): implement catching CTRL+C and handling shutdown gracefully
    satellite_implementation.join();

    return 0;
}
