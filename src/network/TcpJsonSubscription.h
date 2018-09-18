
#pragma once

#include <src/common/Pointers.h>
#include <src/common/StringSpan.h>
#include <src/common/JsonForward.h>
#include <functional>

namespace miner {
    class TcpLineSubscription;

    class TcpJsonSubscription {
    public:
        using OnReceivedJsonFunc = std::function<void (const nl::json &json)>;
        using GetSubscriptionMessageFunc = std::function<void (std::ostream &)>;

        TcpJsonSubscription(cstring_span host, cstring_span port,
                GetSubscriptionMessageFunc &&, OnReceivedJsonFunc &&);
        ~TcpJsonSubscription();

    private:
        unique_ptr<TcpLineSubscription> tcpLines;

        OnReceivedJsonFunc onReceivedJson;
    };

}
