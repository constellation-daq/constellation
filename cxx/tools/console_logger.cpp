/**
 * @file
 * @brief CMDP log receiver
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <any>
#include <map>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include <argparse/argparse.hpp>
#include <asio.hpp>
#include <magic_enum.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/build.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/logging/log.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/core/logging/SinkManager.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::utils;

class LogReceiver {
public:
    LogReceiver(Level min_level, const std::vector<std::string>& topics) : logger_("LOG_RECEIVER") {
        // Register callback
        chirp::Manager::getDefaultInstance()->registerDiscoverCallback(&LogReceiver::callback, chirp::MONITORING, this);
        // Request currently active logging services
        chirp::Manager::getDefaultInstance()->sendRequest(chirp::MONITORING);
        // Add subscriptions for minimum global level
        if(std::to_underlying(min_level) <= std::to_underlying(CRITICAL)) {
            log_topics_.emplace_back("LOG/CRITICAL");
            if(std::to_underlying(min_level) <= std::to_underlying(STATUS)) {
                log_topics_.emplace_back("LOG/STATUS");
                if(std::to_underlying(min_level) <= std::to_underlying(WARNING)) {
                    log_topics_.emplace_back("LOG/WARNING");
                    if(std::to_underlying(min_level) <= std::to_underlying(INFO)) {
                        log_topics_.emplace_back("LOG/INFO");
                        if(std::to_underlying(min_level) <= std::to_underlying(DEBUG)) {
                            log_topics_.emplace_back("LOG/DEBUG");
                            if(std::to_underlying(min_level) <= std::to_underlying(TRACE)) {
                                log_topics_.emplace_back("LOG/TRACE");
                            }
                        }
                    }
                }
            }
        }
        // Add additional topic subscriptions
        for(const auto& topic : topics) {
            log_topics_.emplace_back(transform("LOG/" + topic, ::toupper));
        }
    }

    void callback_impl(chirp::DiscoveredService service, bool depart) {
        const auto uri = "tcp://" + service.address.to_string() + ":" + std::to_string(service.port);
        LOG(logger_, TRACE) << "Callback for " << uri;
        std::unique_lock service_sockets_lock {service_sockets_mutex_};
        if(depart) {
            // Disconnect
            const auto service_socket_it = service_sockets_.find(service);
            if(service_socket_it != service_sockets_.end()) {
                LOG(logger_, DEBUG) << "Disconnecting from " << uri << "...";
                service_socket_it->second.disconnect(uri);
                service_socket_it->second.close();
                service_sockets_.erase(service_socket_it);
                service_sockets_lock.unlock();
                LOG(logger_, INFO) << "Disconnected from " << uri;
            }
        } else {
            // Connect
            LOG(logger_, DEBUG) << "Connecting to " << uri << "...";
            zmq::socket_t socket {context_, zmq::socket_type::sub};
            socket.connect(uri);
            for(const auto& log_topic : log_topics_) {
                LOG(logger_, DEBUG) << "Subscribing to " << log_topic;
                socket.set(zmq::sockopt::subscribe, log_topic);
            }
            service_sockets_.insert(std::make_pair(service, std::move(socket)));
            service_sockets_lock.unlock();
            LOG(logger_, INFO) << "Connected to " << uri;
        }
    }

    static void callback(chirp::DiscoveredService service, bool depart, std::any user_data) {
        auto* instance = std::any_cast<LogReceiver*>(user_data);
        instance->callback_impl(std::move(service), depart);
    }

    void main_loop() {
        while(true) {
            const std::lock_guard service_sockets_lock {service_sockets_mutex_};
            for(auto& [service, socket] : service_sockets_) {
                zmq::multipart_t zmq_msg {};
                auto received = zmq_msg.recv(socket, static_cast<int>(zmq::recv_flags::dontwait));
                if(!received) {
                    continue;
                }
                try {
                    auto cmdp_msg = CMDP1LogMessage::disassemble(zmq_msg);
                    auto msg_info = to_string(cmdp_msg.getHeader().getSender());
                    if(!cmdp_msg.getLogTopic().empty()) {
                        msg_info += "/" + to_string(cmdp_msg.getLogTopic());
                    }
                    LOG(Logger::getDefault(), cmdp_msg.getLogLevel()) << "[" << msg_info << "] " << cmdp_msg.getLogMessage();
                } catch(const MessageDecodingError& error) {
                    LOG(logger_, WARNING) << error.what();
                } catch(const IncorrectMessageType& error) {
                    LOG(logger_, WARNING) << error.what();
                }
            }
        }
    }

private:
    Logger logger_;
    std::vector<std::string> log_topics_;
    zmq::context_t context_;
    std::map<chirp::DiscoveredService, zmq::socket_t> service_sockets_;
    std::mutex service_sockets_mutex_;
};

// NOLINTNEXTLINE(*-avoid-c-arrays)
void parse_args(int argc, char* argv[], argparse::ArgumentParser& parser) {

    // Constellation group (-g)
    parser.add_argument("-g", "--group").help("group name").required();

    // Console log level (-l)
    parser.add_argument("-l", "--level").help("console log level").default_value("INFO");

    // Minimum global subscription level
    parser.add_argument("-r", "--remote-level").help("log level for remote log messages").default_value("WARNING");

    // Topic subscriptions
    parser.add_argument("-t", "--topic").help("additional topic subscriptions (e.g. \"INFO/FSM\")").append();

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
template <typename T = std::string> T get_arg(argparse::ArgumentParser& parser, std::string_view arg) noexcept {
    try {
        return parser.get<T>(arg);
    } catch(const std::exception&) {
        std::unreachable();
    }
}

int main(int argc, char* argv[]) {
    // Get the default logger
    auto& logger = Logger::getDefault();

    // CLI parsing
    argparse::ArgumentParser parser {"cmdp_log_recv", CNSTLN_VERSION};
    try {
        parse_args(argc, argv, parser);
    } catch(const std::exception& error) {
        LOG(logger, CRITICAL) << "Argument parsing failed: " << error.what();
        LOG(logger, CRITICAL) << "Run \"cmdp_log_recv --help\" for help";
        return 1;
    }

    // Set log level
    const auto default_level = magic_enum::enum_cast<Level>(transform(get_arg(parser, "level"), ::toupper));
    if(!default_level.has_value()) {
        LOG(logger, CRITICAL) << "Log level \"" << get_arg(parser, "level") << "\" is not valid"
                              << ", possible values are: " << list_enum_names<Level>();
        return 1;
    }
    SinkManager::getInstance().setGlobalConsoleLevel(default_level.value());

    // Get minimum global log level
    const auto remote_level = magic_enum::enum_cast<Level>(transform(get_arg(parser, "remote-level"), ::toupper));
    if(!remote_level.has_value()) {
        LOG(logger, CRITICAL) << "Log level \"" << get_arg(parser, "remote-level") << "\" is not valid"
                              << ", possible values are: " << list_enum_names<Level>();
        return 1;
    }

    if(std::to_underlying(remote_level.value()) < std::to_underlying(default_level.value())) {
        LOG(logger, WARNING) << "Console log level is higher than log level for remote log messages"
                             << ", some messages might not be printed";
    }

    // Check broadcast and any address
    asio::ip::address_v4 brd_addr {};
    try {
        brd_addr = asio::ip::make_address_v4(get_arg(parser, "brd"));
    } catch(const asio::system_error& error) {
        LOG(logger, CRITICAL) << "Invalid broadcast address \"" << get_arg(parser, "brd") << "\"";
        return 1;
    }
    asio::ip::address_v4 any_addr {};
    try {
        any_addr = asio::ip::make_address_v4(get_arg(parser, "any"));
    } catch(const asio::system_error& error) {
        LOG(logger, CRITICAL) << "Invalid any address \"" << get_arg(parser, "any") << "\"";
        return 1;
    }

    // Create CHIRP manager and set as default
    std::unique_ptr<chirp::Manager> chirp_manager {};
    try {
        // FIXME: make chirp manager work without being a satellite (Issue #36)
        chirp_manager = std::make_unique<chirp::Manager>(brd_addr, any_addr, get_arg(parser, "group"), "cmdp_log_recv");
        chirp_manager->setAsDefaultInstance();
        chirp_manager->start();
    } catch(const std::exception& error) {
        LOG(logger, CRITICAL) << "Failed to initiate network discovery: " << error.what();
        return 1;
    }

    // Start log receiver
    auto log_receiver = LogReceiver(remote_level.value(), get_arg<std::vector<std::string>>(parser, "topic"));
    log_receiver.main_loop();

    return 0;
}
