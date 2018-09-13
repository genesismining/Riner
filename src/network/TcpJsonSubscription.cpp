
#include "TcpJsonSubscription.h"
#include <src/network/TcpLineSubscription.h>
#include <src/common/Json.h>
#include <functional>

namespace miner {

    TcpJsonSubscription::TcpJsonSubscription(cstring_span host, cstring_span port,
                                             GetSubscriptionMessageFunc &&getSubscriptionMessageFunc,
                                             OnReceivedJsonFunc &&onReceivedJsonFunc)
    : onReceivedJson(std::move(onReceivedJsonFunc)) {

        auto onReceivedLine = [this] (auto line) {
            try {
                auto jsonLine = nl::json::parse(line);
                onReceivedJson(jsonLine);
            }
            catch (nl::json::exception &e) {
                LOG(ERROR) << "error while parsing json string: " << e.what()
                           << "\n string: " << gsl::to_string(line);
            }
            catch (std::invalid_argument &e) {
                LOG(ERROR) << "invalid argument while parsing json string: " << e.what()
                           << "\n string: " << gsl::to_string(line);
            }
        };

        tcpLines = std::make_unique<TcpLineSubscription>(host, port,
                std::move(getSubscriptionMessageFunc), onReceivedLine);
    }

    TcpJsonSubscription::~TcpJsonSubscription() {
        //separate definition of the destructor enables forward declaration
        //of unique_ptr<T>'s type T, since its deleter is needed only here
    }

}