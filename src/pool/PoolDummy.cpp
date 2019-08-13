//
//

#include "PoolDummy.h"

namespace miner {

    PoolDummy::PoolDummy(PoolConstructionArgs argsMoved)
            : PoolWithWorkQueue()
            , args(std::move(argsMoved))
            , io(IOMode::Tcp) {

        //launch the json rpc 2.0 client
        //the passed lambda will get called once a connection got established. That connection is represented by cxn.
        //the callback happens on the 'IoThread': a single thread that deals with all the IO of this PoolImpl
        //the connection cxn will only stay open as long as you use it in async io operations such as
        //io.callAsync and io.readAsync etc.
        //to close the connection just stop calling any of these functions on it
        //for more info see io's documentation
        io.launchClientAutoReconnect(args.host, args.port, [this] (CxnHandle cxn) {
            onConnected(cxn);
        });
        //make sure starting the jrpc client is the final thing you do here
        //things that get initialized after the io operations started are not guaranteed to be ready when
        //the lambda gets called from the IoThread
    }

    PoolDummy::~PoolDummy() {
        //implicitly calls io dtor and joins io thread
        //any queued io.callAsync and io.readAsync operations will be aborted and handlers may not get called
    }

    void PoolDummy::onConnected(CxnHandle cxn) {
        LOG(INFO) << "successfully (re)connected";
        acceptWorkMessages = false;

        //jrpc requests can simply be created via the jrpc::RequestBuilder
        //jrpc IDs are not auto-generated, you have control over which ones are used,
        //however the io object does provide a convenience int (io.nextId) that you
        //can increment every time you create a request.
        jrpc::Message subscribe = jrpc::RequestBuilder{}
                .id(io.nextId++)
                .method("mining.subscribe")
                .param("RAIIner")
                .param("gm")
                .done();

        //send the 'subscribe' request over the connection cxn
        //the lambda passed into this function is a response handler, it gets called as soon as a jrpc with matching id
        //gets received from io.readAsync. You are responsible to keep calling readAsync, or use io.readAsyncLoopEnabled.
        //otherwise (if you stop keep calling readAsync) the connection gets dropped
        //see at the end of this function how to do this
        io.callAsync(cxn, subscribe, [this] (CxnHandle cxn, jrpc::Message response) {
            //this handler gets invoked when a jrpc response with the same id as 'subscribe' is received

            //return if it's not a {"result": true} message
            if (!response.isResultTrue()) {
                LOG(INFO) << "mining subscribe is not result true, instead it is " << response.str();
                return;
            }

            //again, make a request and send it via callAsync
            jrpc::Message authorize = jrpc::RequestBuilder{}
                    .id(io.nextId++)
                    .method("mining.authorize")
                    .param(args.username)
                    .param(args.password)
                    .done();

            io.callAsync(cxn, authorize, [this] (CxnHandle cxn, jrpc::Message response) {
                acceptWorkMessages = true;
                //the cxn object can be stored and used with io.callAsync oustide of this function
                _cxn = cxn; //store connection for submit
            });
        });

        //the io object can not only send requests but also answer requests
        //to do so, specify a jrpc method that you want to support
        //incoming jrpc::Messages with a "method" == "mining.notify" will lead to the following lambda being invoked:
        io.addMethod("mining.notify", [this] (nl::json params) {
            if (acceptWorkMessages) {

                //we expect to get 4 unnamed params
                if (params.is_array() && params.size() >= 4)
                    onMiningNotify(params);
                else //throwing a jrpc::Error within this lambda will send a Json RPC 2.0 Error response to the callee
                    throw jrpc::Error{jrpc::invalid_params, "expected at least 4 params"};
            }
        });

        io.setIncomingModifier([&] (jrpc::Message &msg) {
            //called everytime a jrpc::Message is incoming, so that it can be modified (even though we don't modify it here)
            onStillAlive(); //update still alive timer (if a pool does not update its timer for some time, the poolswitcher will treat it as a dead pool)
        });

        //this call below tells the io object to keep scheduling readAsync calls after incoming jrpc::Messages, however it does not
        //schedule a first readAsync call to initiate this loop, you have to manually do that
        io.setReadAsyncLoopEnabled(true);
        io.readAsync(cxn); //initiate the readAsync loop

        //since the io object keeps calling readAsync now, the connection will always be kept alive (unless closed from the other side), to close the
        //connection from this side, a setReadAsyncLoopEnabled(false) is necessary
    }

    void PoolDummy::submitSolutionImpl(unique_ptr<WorkSolution> resultBase) {

    }

    void PoolDummy::onMiningNotify (const nl::json &j) {
    }

}
