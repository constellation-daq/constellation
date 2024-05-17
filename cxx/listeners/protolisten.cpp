/**
 * @file
 * @brief Prototype listener
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <csignal>
#include <functional>
#include <iostream>
#include <stop_token>

#include <argparse/argparse.hpp>
#include <spdlog/details/log_msg.h>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/logging/SinkManager.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/subscriber/SubscriberPool.hpp"

using namespace constellation;

namespace constellation::loglistener {
    class LogListener : public SubscriberPool<message::CMDP1Message> {
    public:
        CNSTLN_API LogListener()
            : SubscriberPool<message::CMDP1Message>(
                  chirp::MONITORING, "LOGGER", std::bind_front(&LogListener::treat_message, this), {"LOG"}),
              logger_("LOGLISTENER") {}
        CNSTLN_API ~LogListener() = default;
        void treat_message(const message::CMDP1Message& msg) {
            if(msg.isLogMessage()) {
                auto header = msg.getHeader();
                auto payload = to_string(msg.getPayloadString());
                auto topic = msg.getTopic();
                // Search topic for second slash after "LOG/" to get substring with log level
                const auto level_endpos = topic.find_first_of('/', 4);
                const auto level_str = topic.substr(4, level_endpos - 4);
                const auto level_opt = magic_enum::enum_cast<Level>(level_str, magic_enum::case_insensitive);
                if(!level_opt.has_value()) {
                    throw MessageDecodingError("\"" + to_string(level_str) + "\" is not a valid log level");
                }
                LOG(logger_, INFO) << "Remote " << msg.getHeader().getSender() << " spoke. "
                                   << " with topic " << to_string(topic) << " with log level " << to_string(level_str)
                                   << " with payload " << payload;
            }
        }

    private:
        Logger logger_;
    };
} // namespace constellation::loglistener

// Use global std::function to work around C linkage
std::function<void(int)> signal_handler_f {}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

extern "C" void signal_hander(int signal) {
    signal_handler_f(signal);
}

void parse_args(int argc, char* argv[], argparse::ArgumentParser& parser) {
    // Listener name (-n)
    parser.add_argument("-n", "--name").help("listener name").default_value("protolisten");

    // Constellation group (-g)
    parser.add_argument("-g", "--group").help("group name").required();

    // Console log level (-l)
    parser.add_argument("-l", "--level").help("log level").default_value("INFO");

    // Subscription level (-l)
    parser.add_argument("-s", "--subscription").help("subscription log level").default_value("INFO");

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

int main(int argc, char** argv) {

    log::Logger logger {"log_receiver"};
    // // or get the default logger=
    // auto& logger = Logger::getDefault();

    // CLI parsing
    argparse::ArgumentParser parser {"protolisten", CNSTLN_VERSION};
    try {
        parse_args(argc, argv, parser);
    } catch(const std::exception& error) {
        LOG(logger, CRITICAL) << "Argument parsing failed: " << error.what();
        LOG(logger, CRITICAL) << "Run \""
                              << "protolisten"
                              << " --help\" for help";
        return 1;
    }

    // Retrieve name and group from the parser
    const auto listener_name = get_arg(parser, "name");
    const auto listener_group = get_arg(parser, "group");
    const auto canonical_name = "protolisten." + listener_name;

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

    LOG(logger, STATUS) << "Prototype listener " << listener_name << " started in Constellation group " << listener_group;

    auto chirp_manager = chirp::Manager(brd_addr, any_addr, listener_group, canonical_name);
    chirp_manager.setAsDefaultInstance();
    chirp_manager.start();

    // This does the magic:
    const loglistener::LogListener receiver;

    std::stop_source stop_token;
    signal_handler_f = [&](int /*signal*/) -> void { stop_token.request_stop(); };

    // NOLINTBEGIN(cert-err33-c)
    std::signal(SIGTERM, &signal_hander);
    std::signal(SIGINT, &signal_hander);
    // NOLINTEND(cert-err33-c)

    while(!stop_token.stop_requested()) {
        std::this_thread::sleep_for(100ms);
    }

    return 0;
}
