#include <csignal>
#include <functional>
#include <iostream>
#include <stop_token>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/subscriber/Subscriber.hpp"

namespace constellation::listener {
    class LogListener : public Subscriber<message::CMDP1Message> {
    public:
        CNSTLN_API LogListener()
            : Subscriber<message::CMDP1Message>(
                  chirp::MONITORING, "LOGGER", std::bind_front(&LogListener::treat_message, this), {"LOG"}),
              logger_("THELOGGR") {}
        CNSTLN_API ~LogListener() = default;
        void treat_message(const message::CMDP1Message& msg) {
            if(msg.isLogMessage()) {
                LOG(logger_, INFO) << "Remote " << msg.getHeader().getSender() << " spoke. ";
                //<< reinterpret_cast<const message::CMDP1LogMessage>(msg).getLogMessage();
            }
        }

    private:
        Logger logger_;
    };
} // namespace constellation::listener

// Use global std::function to work around C linkage
std::function<void(int)> signal_handler_f {}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

extern "C" void signal_hander(int signal) {
    signal_handler_f(signal);
}

using namespace constellation;
int main(int argc, char* argv[]) {
    // Get address via cmdline
    if(argc != 2) {
        std::cout << "Invalid usage: log_receiver CONSTELLATION_GROUP" << std::endl;
        return 1;
    }

    auto chirp_manager = chirp::Manager("255.255.255.255", "0.0.0.0", argv[1], "chp_receiver");
    chirp_manager.setAsDefaultInstance();
    chirp_manager.start();

    log::Logger logger {"log_receiver"};

    // This does the magic:
    const listener::LogListener receiver;

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
