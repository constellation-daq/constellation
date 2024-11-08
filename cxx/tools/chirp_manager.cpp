/**
 * @file
 * @brief Example implementation of CHIRP manager
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <any>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <asio.hpp>
#include <magic_enum.hpp>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/log/SinkManager.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::chirp;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::utils;
using namespace std::string_literals;

enum class Command : std::uint8_t {
    list_registered_services,
    list_discovered_services,
    register_service,
    unregister_service,
    register_callback,
    unregister_callback,
    request,
    reset,
    quit,
};
using enum Command;

namespace {
    template <typename T> std::string pad_str_right(const T& string, std::size_t width) {
        std::string out {string.data(), string.size()};
        while(out.size() < width) {
            out += ' ';
        }
        return out;
    }

    // DiscoverCallback signature NOLINTNEXTLINE(performance-unnecessary-value-param)
    void discover_callback(DiscoveredService service, ServiceStatus status, std::any /* user_data */) {
        std::cout << "Callback:\n"
                  << " Service " << pad_str_right(magic_enum::enum_name(service.identifier), 10) //
                  << " Port " << std::setw(5) << service.port                                    //
                  << " Host " << service.host_id.to_string()                                     //
                  << " IP " << pad_str_right(service.address.to_string(), 15)                    //
                  << to_string(status)                                                           //
                  << "\n"
                  << std::flush;
    }

    void cli_loop(std::span<char*> args) {
        // Get constellation group, name, brd address, and any address via cmdline
        std::cout << "Usage: chirp_manager CONSTELLATION_GROUP NAME BRD_ADDR ANY_ADDR\n" << std::flush;

        auto group = "constellation"s;
        auto name = "chirp_manager"s;
        auto brd_address = asio::ip::address_v4::broadcast();
        auto any_address = asio::ip::address_v4::any();
        if(args.size() >= 2) {
            group = args[1];
        }
        std::cout << "Using constellation group " << std::quoted(group) << "\n" << std::flush;
        if(args.size() >= 3) {
            name = args[2];
        }
        if(args.size() >= 4) {
            try {
                brd_address = asio::ip::make_address_v4(args[3]);
            } catch(const asio::system_error& error) {
                std::cerr << "Unable to use specified broadcast address " << std::quoted(args[3])
                          << ", using default instead\n"
                          << std::flush;
            }
        }
        if(args.size() >= 5) {
            try {
                any_address = asio::ip::make_address_v4(args[4]);
            } catch(const asio::system_error& error) {
                std::cerr << "Unable to use specified any address " << std::quoted(args[4]) << ", using default instead\n"
                          << std::flush;
            }
        }

        // Turn off console logging
        SinkManager::getInstance().setConsoleLevels(OFF);

        Manager manager {brd_address, any_address, group, name};

        std::cout << "Commands: "
                  << "\n list_registered_services"
                  << "\n list_discovered_services <ServiceIdentifier>"
                  << "\n register_service <ServiceIdentifier:CONTROL> <Port:23999>"
                  << "\n unregister_service <ServiceIdentifier:CONTROL> <Port:23999>"
                  << "\n register_callback <ServiceIdentifier:CONTROL>"
                  << "\n unregister_callback <ServiceIdentifier:CONTROL>"
                  << "\n request <ServiceIdentifier:CONTROL>"
                  << "\n reset" << "\n"
                  << std::flush;

        manager.start();

        while(true) {
            std::string cmd_input {};
            std::getline(std::cin, cmd_input);

            // Split command by spaces to vector of string views
            std::vector<std::string_view> cmd_split {};
            for(const auto word_range : std::ranges::split_view(cmd_input, ' ')) {
                cmd_split.emplace_back(std::ranges::cdata(word_range), std::ranges::size(word_range));
            }

            // If not a command, continue
            if(cmd_split.empty()) {
                continue;
            }
            auto cmd_opt = magic_enum::enum_cast<Command>(cmd_split[0]);
            if(!cmd_opt.has_value()) {
                std::cout << std::quoted(cmd_split[0]) << " is not a valid command\n" << std::flush;
                continue;
            }
            auto cmd = cmd_opt.value();

            // List registered services
            if(cmd == list_registered_services) {
                auto registered_services = manager.getRegisteredServices();
                std::cout << " Registered Services:\n";
                for(const auto& service : registered_services) {
                    std::cout << " Service " << pad_str_right(magic_enum::enum_name(service.identifier), 10) //
                              << " Port " << std::setw(5) << service.port                                    //
                              << "\n";
                }
                std::cout << std::flush;
                continue;
            }
            // List discovered services
            if(cmd == list_discovered_services) {
                std::optional<ServiceIdentifier> service_opt {std::nullopt};
                if(cmd_split.size() >= 2) {
                    service_opt = magic_enum::enum_cast<ServiceIdentifier>(cmd_split[1]);
                }
                auto discovered_services = service_opt.has_value() ? manager.getDiscoveredServices(service_opt.value())
                                                                   : manager.getDiscoveredServices();
                std::cout << " Discovered Services:\n";
                for(const auto& service : discovered_services) {
                    std::cout << " Service " << pad_str_right(magic_enum::enum_name(service.identifier), 15) //
                              << " Port " << std::setw(5) << service.port                                    //
                              << " Host " << service.host_id.to_string()                                     //
                              << " IP " << pad_str_right(service.address.to_string(), 15)                    //
                              << "\n";
                }
                std::cout << std::flush;
                continue;
            }
            // Register or unregister a service
            if(cmd == register_service || cmd == unregister_service) {
                ServiceIdentifier service {CONTROL};
                if(cmd_split.size() >= 2) {
                    service = magic_enum::enum_cast<ServiceIdentifier>(cmd_split[1]).value_or(CONTROL);
                }
                Port port {23999};
                if(cmd_split.size() >= 3) {
                    std::from_chars(cmd_split[2].data(), cmd_split[2].data() + cmd_split[2].size(), port);
                }
                if(cmd == register_service) {
                    auto ret = manager.registerService(service, port);
                    if(ret) {
                        std::cout << " Registered Service " << pad_str_right(magic_enum::enum_name(service), 10) //
                                  << " Port " << std::setw(5) << port << "\n"
                                  << std::flush;
                    }
                } else {
                    auto ret = manager.unregisterService(service, port);
                    if(ret) {
                        std::cout << " Unregistered Service " << pad_str_right(magic_enum::enum_name(service), 10) //
                                  << " Port " << std::setw(5) << port << "\n"
                                  << std::flush;
                    }
                }
                continue;
            }
            // Register of unregister callback
            if(cmd == register_callback || cmd == unregister_callback) {
                ServiceIdentifier service {CONTROL};
                if(cmd_split.size() >= 2) {
                    service = magic_enum::enum_cast<ServiceIdentifier>(cmd_split[1]).value_or(CONTROL);
                }
                if(cmd == register_callback) {
                    auto ret = manager.registerDiscoverCallback(&discover_callback, service, nullptr);
                    if(ret) {
                        std::cout << " Registered Callback for " << magic_enum::enum_name(service) << "\n" << std::flush;
                    }
                } else {
                    auto ret = manager.unregisterDiscoverCallback(&discover_callback, service);
                    if(ret) {
                        std::cout << " Unregistered Callback for " << magic_enum::enum_name(service) << "\n" << std::flush;
                    }
                }
                continue;
            }
            // Send CHIRP request
            if(cmd == request) {
                ServiceIdentifier service {CONTROL};
                if(cmd_split.size() >= 2) {
                    service = magic_enum::enum_cast<ServiceIdentifier>(cmd_split[1]).value_or(CONTROL);
                }
                manager.sendRequest(service);
                std::cout << " Sent Request for " << magic_enum::enum_name(service) << "\n" << std::flush;
                continue;
            }
            // Reset
            if(cmd == reset) {
                manager.unregisterDiscoverCallbacks();
                manager.unregisterServices();
                manager.forgetDiscoveredServices();
                continue;
            }
            // Quit
            if(cmd == quit) {
                break;
            }
        }
    }
} // namespace

int main(int argc, char* argv[]) {
    try {
        cli_loop(std::span(argv, argc));
    } catch(...) {
        return 1;
    }
    return 0;
}
