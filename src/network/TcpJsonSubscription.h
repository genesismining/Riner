
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
        using OnSendMessageFunc  = std::function<void (std::ostream &)>;

        TcpJsonSubscription(cstring_span host, cstring_span port,
                            OnSendMessageFunc &&, OnReceivedJsonFunc &&);
        ~TcpJsonSubscription();

        void sendAsync(OnSendMessageFunc &&);

    private:
        unique_ptr<TcpLineSubscription> tcpLines;

        OnReceivedJsonFunc onReceivedJson;
    };

}
