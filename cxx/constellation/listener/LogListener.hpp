/**
 * @file
 * @brief Subscriber pool for CMDP logging
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "constellation/build.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/listener/CMDPListener.hpp"

namespace constellation::listener {

    class CNSTLN_API LogListener : public CMDPListener {
    public:
        /**
         * @brief Construct log listener
         *
         * @param log_topic Logger topic to be used for this component
         * @param callback Callback function pointer for received messages
         */
        CNSTLN_API LogListener(std::string_view log_topic, std::function<void(message::CMDP1LogMessage&&)> callback);

        CNSTLN_API void setGlobalLogLevel(log::Level level);
        CNSTLN_API log::Level getGlobalLogLevel() const;

        CNSTLN_API void subscribeLogTopic(const std::string& log_topic, log::Level level);
        CNSTLN_API void unsubscribeLogTopic(const std::string& log_topic);
        CNSTLN_API std::map<std::string, log::Level> getLogTopicSubscriptions();

        CNSTLN_API void subscribeExtaLogTopic(const std::string& host, const std::string& log_topic, log::Level level);
        CNSTLN_API void unsubscribeExtraLogTopic(const std::string& host, const std::string& log_topic);
        CNSTLN_API std::map<std::string, log::Level> getExtraLogTopicSubscriptions(const std::string& host);

    private:
        CNSTLN_LOCAL static std::vector<std::string> generate_topics(const std::string& log_topic, log::Level level);
        CNSTLN_LOCAL static std::pair<std::string_view, log::Level> demangle_topic(std::string_view topic);

        // Hide subscribe/unsubscribe functions from CMDPListener
        // clang-format off
        using CMDPListener::getExtraTopicSubscriptions;
        using CMDPListener::getTopicSubscriptions;
        using CMDPListener::setExtraTopicSubscriptions;
        using CMDPListener::setTopicSubscriptions;
        using CMDPListener::subscribeExtraTopic;
        using CMDPListener::subscribeTopic;
        using CMDPListener::unsubscribeExtraTopic;
        using CMDPListener::unsubscribeTopic;
        using CMDPListener::removeExtraTopicSubscriptions;
        // clang-format on

    private:
        std::atomic<log::Level> global_log_level_;
    };

} // namespace constellation::listener
