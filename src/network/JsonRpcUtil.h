//
//

#pragma once

#include "JsonRpcMethod.h"
#include "JsonRpcHandlerMap.h"
#include "JsonRpcInvoke.h"
#include "JsonRpcBuilder.h"
#include "JsonRpcIO.h"
#include <src/util/LockUtils.h>
#include <deque>

namespace miner { namespace jrpc {

        class JsonRpcUtil : private JsonRpcIO { //inherit privately instead of having a member to expose certain parts of JsonRpcIO's api via using Base::method
            using Base = JsonRpcIO;

            std::deque<Method> _methods;
            HandlerMap _pending; //Note: it would make more sense to have one HandlerMap per connection (map<CxnHandle, HandlerMap>), but at this point thats not necessary yet.

            bool _readAsyncLoopEnabled = false;

            bool hasMethod(const char *name) const;

        public:
            //expose the following functions from JsonRpcIO
            using Base::launchClient;
            using Base::launchClientAutoReconnect;
            using Base::launchServer;
            using Base::setIncomingModifier;
            using Base::setOutgoingModifier;
            using Base::retryAsyncEvery;
            using Base::postAsync;
            using Base::readAsync;
            //don't expose writeAsync, use callAsync instead

            JsonRpcIO &io() {//if you really want access you can have it
                return *this;
            }

            ~JsonRpcUtil() {
                stopIOThread();
            }

            //if ReadAsyncLoop is enabled (true) this object keeps scheduling readAsync() calls after a message was successfully read (via readAsync()) to keep the connection alive and receive further messages
            //this means you usually want to call setReadAsyncLoopEnabled(true) followed by a readAsync(cxn) call that initiates the loop
            //this function may only be called before a launchClient/Server call or from the io-thread
            //it also means that setReadAsyncLoopEnabled(false) becomes something like a 'disconnect' call when used in a received message callback, as no further requests are enqueued
            void setReadAsyncLoopEnabled(bool val);

            using IdType = int64_t;
            std::atomic<IdType> nextId = {0}; //expose id counter publicly because its really helpful

            explicit JsonRpcUtil(IOMode mode);

            template<class Func, class ... ArgNames>
            void addMethod(const char *name, Func &&func, ArgNames &&... argNames) {
                MI_EXPECTS(!hasMethod(name)); //method overloading not supported

                //method overloading not supported, same-name methods are likely added after reconnect
                _methods.erase(std::remove_if(_methods.begin(), _methods.end(), [name] (const Method &method) {
                    return method.name() == name;
                }), _methods.end());

                //add new method
                _methods.emplace_back(name,
                        wrapFunc(std::forward<Func>(func), std::forward<ArgNames>(argNames)...));

            }

            void callAsync(CxnHandle, Message request, ResponseHandler &&handler = responseHandlerNoop);

            void callAsyncRetryNTimes(CxnHandle, Message request, uint32_t maxTries, milliseconds retryInterval, ResponseHandler &&handler, std::function<void()> neverRespondedHandler = [] () {});
        };

}}