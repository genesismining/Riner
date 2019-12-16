//
//

#include <src/util/HexString.h>
#include "PoolDummy.h"
#include "PoolEthash.h"

namespace miner {

    PoolDummy::PoolDummy(const PoolConstructionArgs &args)
            : Pool(args) //set args in base class, can be accessed via this->constructionArgs
            , io(IOMode::Tcp) {

        tryConnect();
        //make sure tryConnect() is the final thing you do in this ctor.
        //things that get initialized after the io operations started are not guaranteed to be ready when
        //callbacks are getting called from the IoThread
    }

    void PoolDummy::tryConnect() {
        auto &args = this->constructionArgs; //get args from base class
        io.launchClientAutoReconnect(args.host, args.port, [this] (CxnHandle cxn) {
            onConnected(cxn);
        });
        //launch the json rpc 2.0 client
        //the passed lambda will get called once a connection got established. That connection is represented by cxn.
        //the callback happens on the 'IoThread': a single thread that deals with all the IO of this PoolImpl owned by io
        //the connection cxn will only stay open as long as you use it in async io operations such as
        //io.callAsync and io.readAsync etc.
        //to close the connection just stop calling any of these functions on it
        //for more info see io's documentation
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
                    .param(constructionArgs.username)
                    .param(constructionArgs.password)
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

        //since the io object keeps calling readAsync from now on, the connection will be kept alive (unless actively closed), to close the
        //connection from this side, a setReadAsyncLoopEnabled(false) is sufficient.
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
        auto solution = static_unique_ptr_cast<WorkSolutionEthash>(std::move(solutionBase));

        //this function must construct and send a submit message on the ioThread.
        //to achieve that, enqueue a lambda to the ioThread via io.postAsync
        //the ioThread will run this lambda as soon as possible.
        //postAsync does not block, so we can swiftly return from the submitSolutionImpl function.
        //you should not let this function block for too long.

        io.postAsync([this, solution = move(solution)] { //move capture the solution, so it is alive until submission

            //get the PoolJob as its dynamic type EthashStratumJob, so that we can take the stratum jobId from it
            auto job = solution->tryGetJobAs<EthashStratumJob>();
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
                    .param("0x" + HexString(toBytesWithBigEndian(solution->nonce)).str()) //nonce must be big endian
                    .param("0x" + HexString(solution->header).str())
                    .param("0x" + HexString(solution->mixHash).str())
                    .done(); //call "done()" to convert the RequestBuilder to a jrpc::Message

            //this lambda will get called if we get a response, either the share was accepted or rejected
            auto onResponse = [this, difficulty = solution->jobDifficulty] (CxnHandle cxn, jrpc::Message response) {
                //
                records.reportShare(difficulty, response.isResultTrue(), false);
                std::string acceptedStr = response.isResultTrue() ? "accepted" : "rejected";
                LOG(INFO) << "share with id '" << response.id << "' got " << acceptedStr;
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
        //parse the jparams and create a PoolJob object
        //push that PoolJob object into the WorkQueue so it can be divided into individual Work objects

        bool cleanFlag = jparams.at(4);
        const auto &jobId = jparams.at(0).get<std::string>();

        auto job = std::make_unique<EthashStratumJob>(_this, jobId);
        HexString(jparams[1]).getBytes(job->workTemplate.header);
        HexString(jparams[2]).getBytes(job->workTemplate.seedHash);

        Bytes<32> jobTarget;
        HexString(jparams[3]).swapByteOrder().getBytes(jobTarget);

        job->workTemplate.setDifficultiesAndTargets(jobTarget);

        //workTemplate->epoch will be calculated in EthashStratumJob::makeWork()
        //so that not too much time is spent on this thread.

        queue.pushJob(std::move(job), cleanFlag);
        //job will now be used (via job->makeWork()) to generate new Work instances for AlgoImpls that call pool.tryGetWork())
    }

}
