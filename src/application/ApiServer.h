
#pragma once

#include <src/common/Json.h>
#include <src/common/Pointers.h>
#include <src/util/LockUtils.h>
#include <src/network/JsonRpcUtil.h>
#include "Device.h"
#include "Application.h"
#include <deque>
#include <map>

namespace riner {
    class Application;

    /**
     * runs a Json RPC 2.0 api which exposes the methods described in the `registerFunctions()` implementation
     */
    class ApiServer {
        const Application &_app; //app owns the ApiServer, therefore app outlives the Apiserver, which makes this ref safe

        unique_ptr<jrpc::JsonRpcUtil> io;
        
        /**
         * take a look at this function's implementation to see which methods are available and/or add methods
         */
        void registerFunctions();
    public:
        /**
         * Start the ApiServer on the local machine and start listening on port `port`.
         * The ApiServer will start running as soon as the constructor is called and will join its thread in the (blocking) destructor call, which means the ApiServer is alive as long as the instance is alive.
         * param port the port on which the ApiServer should listen for incoming connections (usually provided by the user in the `Config`'s general settings
         * param app reference that goes back to the application object (which the ApiServer has friend access rights to, in order to communicate the application's state in detail)
         */
        explicit ApiServer(uint16_t port, const Application &app);
        
        /**
         * blocking, terminates all connections and joins the ApiServer io thread.
         */
        ~ApiServer();
    };

}
