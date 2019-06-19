//
//

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

            std::atomic<int64_t> nextId = {0}; //expose id counter publicly because its really helpful

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

            void callAsync(CxnHandle, Message request, ResponseHandler &&handler);

            void callAsyncRetryNTimes(CxnHandle, Message request, uint32_t maxTries, milliseconds retryInterval, ResponseHandler &&handler, std::function<void()> neverRespondedHandler = [] () {});
        };

}}