
#include <iostream>

#include "Application.h"
#include <src/network/TcpJsonSubscription.h>

#include <src/util/Logging.h>
INITIALIZE_EASYLOGGINGPP

#include <thread>

namespace miner {

    Application::Application(int argc, char *argv[]) {
        START_EASYLOGGINGPP(argc, argv);

        auto user = "user123", password = "password123";

        auto subscribe = [user, password] (std::ostream &stream) {
            stream << R"({"jsonrpc": "2.0", "method" : "mining.subscribe", "params" : {"username": ")"
                      << user << R"(", "password": ")" << password << R"("}, "id": 1})" << "\n";
        };

        TcpJsonSubscription tcpJson = {"eth-eu1.nanopool.org", "9999", subscribe, [] (auto &json) {

            auto m = json.find("method");
            if (m != json.end() && *m == "mining.notify") {

                LOG(INFO) << "received json: " << json.dump(4);

            }
        }};

        for (size_t i = 0; i < 20; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            LOG(INFO) << "...";
        }
    }

}









