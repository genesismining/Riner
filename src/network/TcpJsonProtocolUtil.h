
#pragma once

#include <src/network/TcpLineProtocolUtil.h>
#include <src/common/Pointers.h>
#include <src/common/StringSpan.h>
#include <src/common/JsonForward.h>
#include <functional>

namespace miner {
    class TcpLineProtocolUtil;

    class TcpJsonProtocolUtil {
    public:
        using OnEventFunc = std::function<void(nl::json responseJson, const asio::error_code &, asio::coroutine &)>;

        TcpJsonProtocolUtil(cstring_span host, cstring_span port, OnEventFunc &&onEvent);
        ~TcpJsonProtocolUtil();

        template<class Fn>
        void postAsync(Fn &&func) {
            tcpLines->postAsync(std::forward<Fn>(func));
        }

        void launch();

        void asyncWrite(std::string request, bool reenterCoroutine = false);
        void asyncWrite(nl::json request, bool reenterCoroutine = false);

        void asyncRead();

        //retries calling the function after a time interval until it returns true for the first time
        void asyncRetryEvery(std::chrono::milliseconds interval, std::function<bool()> &&pred);

    private:

        unique_ptr<TcpLineProtocolUtil> tcpLines;

        OnEventFunc onEvent;

    };

}