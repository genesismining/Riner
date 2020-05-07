//
//

#include <src/util/HexString.h>
#include "PoolDummy.h"
#include "WorkDummy.h"

namespace riner {

    PoolDummy::PoolDummy(const PoolConstructionArgs &args)
            : Pool(args) //set args in base class, can be accessed via this->constructionArgs
            , mock_server(args.port) //for the sake of this demonstration we also launch a fake server on the port we later want to connect to, so that you can see the PoolDummy successfully connecting to something and actually getting work messages
            {

        //if the config says we want an SSL connection we have to make the necessary preparations here.
        if (args.sslDesc.client) {
            io.io().enableSsl(args.sslDesc); //we decide to use the jrpc::JsonRpcUtil object "io" for
            //our io, so we have to forward it all the necessary info for establishing a SSL connection.
            //If you use another kind of network io, you have to forward it to that
        }

        tryConnect();
        //make sure tryConnect() is the final thing you do in this ctor.
        //things that get initialized after the io operations started are not guaranteed to be ready when
        //callbacks are getting called from the IoThread
    }

    void PoolDummy::tryConnect() {
        setConnected(false);
        if (isDisabled() || !isActive()) {
            return;
        }
        auto &args = this->constructionArgs; //the Pool base class stores the constructionArgs for us, we don't need to store them.

        //for demonstration purposes we pass 127.0.0.1 instead of `args.host` to the launch method, so it connects with this->mock_server.
        io.launchClientAutoReconnect("127.0.0.1" /*args.host*/, args.port,
                [this] (CxnHandle cxn) { //on connected
            onConnected(cxn); //forward the call to our member function
        },
        [this] () { //on disconnected
            setConnected(false); //mark pool as disconnected
        });
        //launch the json rpc 2.0 client
        //the first passed lambda will get called once a connection got established. That connection is represented by cxn.
        //the callback happens on the 'IoThread': a single thread that deals with all the IO of this PoolImpl owned by io.
        //the connection cxn will only stay open as long as you use it in async io operations such as
        //io.callAsync and io.readAsync etc.
        //to close the connection just stop calling any of these functions on it.
        //the second lambda is called whenever the connection is dropped
        //for more info see io's documentation
    }

    PoolDummy::~PoolDummy() {
        //implicitly calls io dtor and joins io thread
        //any queued io.callAsync and io.readAsync operations will be aborted and handlers may not get called
    }

    void PoolDummy::onConnected(CxnHandle cxn) {
        VLOG(0) << "PoolDummy successfully (re)connected";

        acceptWorkMessages = false; //first we don't accept incoming work messages,
        //later when the pool has accepted our "mining.authorize" call we will change that

        //jrpc requests can simply be created via the jrpc::RequestBuilder
        //jrpc IDs are not auto-generated, you have control over which ones are used,
        //however the io object does provide a convenience int (io.nextId) that you
        //can increment every time you create a request.
        jrpc::Message subscribe = jrpc::RequestBuilder{}
                .id(io.nextId++)
                .method("mining.subscribe")
                .param("riner")
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
                LOG(INFO) << "mining subscribe failed: " << response.str();
                setDisabled(true); //disable pool because other subscribe attempts will most probably fail, too
                return;
            }

            //again, make a request and send it via callAsync
            jrpc::Message authorize = jrpc::RequestBuilder{}
                    .id(io.nextId++)
                    .method("mining.authorize")
                    .param(constructionArgs.username)
                    .param(constructionArgs.password)
                    .done();

            io.callAsync(cxn, authorize, [this] (CxnHandle cxn, jrpc::Message response) {
                if (response.isResultTrue()) {
                    acceptWorkMessages = true;
                    //the cxn object can be stored and used with io.callAsync oustide of this function
                    _cxn = cxn; //store connection for submitSolution later

                    LOG(INFO) << "waiting for first job from pool server... (takes few seconds in this example)";
                }
                else {
                    LOG(INFO) << "Pool disabled because authorization failed (" << response.str() << "). Please check whether the correct pool credentials are supplied";
                    setDisabled(true);
                }
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

        //since the io object keeps calling readAsync from now on, the connection will be kept alive (unless actively closed), to close the
        //connection from this side, a setReadAsyncLoopEnabled(false) is sufficient.
    }

    void PoolDummy::expireJobs() {
        queue.expireJobs();
    }

    void PoolDummy::clearJobs() {
        queue.clear();
    }

    bool PoolDummy::isExpiredJob(const PoolJob &job) {
        //ask the workqueue whether job is the most recent job pushed into the queue (via queue.pushJob(...))
        return queue.isExpiredJob(job);
    }

    unique_ptr<Work> PoolDummy::tryGetWorkImpl() {
        //the queue has internally (on its own thread) called job->makeWork() multiple times to create lots of work objects from a
        //job (that is, only if a job was already pushed via queue.pushJob(...)).
        //the function 'popWithTimeout' returns one of these Work objects, or returns nullptr if no job was created yet/within the timeout.
        //since the callee of this function checks for nullptr, we don't have to check for that here.
        return queue.popWithTimeout();
    }

    void PoolDummy::onDeclaredDead() {
        io.disconnectAll(); //disconnect if there is still a connection
        tryConnect(); //try to reconnect so that `onStillAlive()` may get called again (see above) and the pool can wake up again
    }

    void PoolDummy::submitSolutionImpl(unique_ptr<WorkSolution> solutionBase) {
        //this function obtains a generic WorkSolution, but our pool knows that it must be a WorkSolutionEthash, because this is an Ethash Pool
        auto solution = static_unique_ptr_cast<WorkSolutionDummy>(std::move(solutionBase));

        //this function must construct and send a submit message on the ioThread.
        //to achieve that, enqueue a lambda to the ioThread via io.postAsync
        //the ioThread will run this lambda as soon as possible.
        //postAsync does not block, so we can swiftly return from the submitSolutionImpl function.
        //you should not let this function block for too long.

        io.postAsync([this, solution = move(solution)] { //move capture the solution, so it is alive until submission

            //get the PoolJob as its dynamic type EthashStratumJob, so that we can take the stratum jobId from it
            auto job = solution->tryGetJobAs<DummyPoolJob>();
            if (!job) {
                LOG(INFO) << "ethash solution cannot be submitted because its PoolJob has expired";
                return; //work has expired, cancel
            }

            //the json rpc io object provides an id counter, because that is always useful wenn sending json rpcs
            //here we use that id counter to obtain new jrpc ids that we redefine as our submission (share) ids.
            //every work solution submission has a unique share id, and thus we can track which shares are accepted/rejected/stale.
            uint32_t shareId = io.nextId++;

            //build the mining.submit json rpc
            jrpc::Message submit = jrpc::RequestBuilder{}
                    .id(shareId)
                    .method("mining.submit")
                    .param(constructionArgs.username)
                    .param(job->jobId)
                    .param("0x" + HexString(toBytesWithBigEndian(solution->nonce)).str()) //just to show some of the HexString and endian helpers we provide
                    .done(); //call "done()" to convert the RequestBuilder to a jrpc::Message

            //this lambda will get called if we get a response, either the share was accepted or rejected
            auto onResponse = [this, difficulty = job->difficulty] (CxnHandle cxn, jrpc::Message response) {
                //
                records.reportShare(difficulty, response.isResultTrue(), false);
                std::string acceptedStr = response.isResultTrue() ? "accepted" : "rejected";
                LOG(INFO) << "share with id '" << response.id << "' got " << acceptedStr << " by '" << getName() << "'";
            };

            //the following lambda gets called if there was no response even after the io object tried to send multiple times.
            //(the lambda will also get called if the pool gets shutdown before the last try)
            auto onNeverResponded = [shareId] () {
                LOG(INFO) << "share with id " << shareId << " got discarded after pool did not respond multiple times";
            };

            //this call below tries to send the `submit` request.
            //if there is no response, after the specified amount of seconds, it will try to send again (up to maxTries times)
            //if there is still no response after the last try, the onNeverResponded lambda gets called.
            auto maxTries = 5;
            io.callAsyncRetryNTimes(_cxn, submit, maxTries, seconds(5), onResponse, onNeverResponded);

        });
    }

    void PoolDummy::onMiningNotify (const nl::json &jparams) {
        //parse the json message params "jparams" and create a PoolJob object
        //push that PoolJob object into the WorkQueue so it can be divided into individual Work objects

        //you can parse json with the nlohmann::json library (see their documentation)
        const auto &jobId = jparams.at(0).get<std::string>();

        //make a new job as a unique_ptr so you can use it with queue.pushJob down below
        auto job = std::make_unique<DummyPoolJob>(_this, jobId);

        //this is an example of the HexString utilities we provide, which you will probably need in your onMiningNotify
        HexString(jparams[1]).getBytes(job->workTemplate.header);

        //parse json param 3 as hex-string, swap the byte order and write it to workTemplate's target.
        HexString(jparams[3]).swapByteOrder().getBytes(job->workTemplate.target);

        //helper function to calculate approximate difficulty from target
        job->difficulty = targetToDifficultyApprox(job->workTemplate.target);

        bool shouldClean = false; //you can set this to true to clear all existing
        // contents of the workQueue. That way AlgoImpls won't pop any older work from now on

        //mark pool as connected because the first job was received
        setConnected(true);
        //add job to job queue

        LOG(INFO) << "we have a new job! (and therefore a new WorkTemplate)";
        queue.pushJob(std::move(job), shouldClean);
        //now that the job is pushed, `queue`'s background thread will every now and then call
        //job->makeWork() to produce enough work objects.
        //these work objects will be handed out in "PoolDummy::tryGetWorkImpl()" above.
    }

}
