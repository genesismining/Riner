//
//

#pragma once

#include <src/common/Optional.h>
#include <string>
#include <vector>
#include <src/util/Logging.h>

namespace miner {

    /**
     * This descriptor is used in the IOTypeLayer's enableSsl(desc) method.
     * To add further parametrization to the Ssl functionality, enhance this SslDesc class
     * and use it in Socket.cpp (which contains all the code related to SSL/TLS via asio)
     */
    struct SslDesc {
        std::vector<std::string> certFiles; //if no cert file is provided, asio's set_default_verify_paths() is used

        struct Server {
            std::string certificateChainFile;
            std::string privateKeyFile;
            std::string tmpDhFile;
            std::function<std::string()> onGetPassword = [] () {LOG(INFO) << "no password callback defined in SslDesc, using empty string."; return "";};
        };
        optional<Server> server;
    };

}