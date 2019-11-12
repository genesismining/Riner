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
        struct Client {
            optional<std::string> certFile; //if no cert file is provided, asio's set_default_verify_paths() is used
        };

        struct Server { //settings that are exclusively relevant for the server
            std::string certificateChainFile;
            std::string privateKeyFile;
            std::string tmpDhFile;
            std::function<std::string()> onGetPassword = [] () {LOG(INFO) << "no password callback defined in SslDesc, using empty string."; return "";};
        };

        //once variant is less syntactically heavy to use we can change this to a variant<Client, Server>
        optional<Client> client;
        optional<Server> server;
    };

}