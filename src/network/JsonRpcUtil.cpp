//
//

#include "JsonRpcUtil.h"

namespace riner { namespace jrpc {

        JsonRpcUtil::JsonRpcUtil(const char *customIoThreadName, IOMode mode) : Base(customIoThreadName, mode) {

            setOnReceive([this] (CxnHandle cxn, Message msg) {

                if (msg.isRequest()) {
                    Message response;
                    try {
                        response = invokeMatchingMethod(_methods, msg);
                    }
                    catch(const nl::json::exception &e) {
                        response = Message{Response{Error{internal_error, "json exception in invoked method"}}, msg.id};
                    }

                    //respond only if its not a notification or if a notification called a method that wasn't found
                    if (!msg.isNotification() || response.isErrorWithCode(jrpc::method_not_found))
                        writeAsync(cxn, std::move(response));
                }
                else if (msg.isResponse()) {

                    if (auto optHandler = _pending.tryPopForId(msg.id)) {
                        try {
                            (*optHandler)(cxn, msg);
                        }
                        catch (const nl::json::exception &e) {
                            LOG(ERROR) << "json error while running jrpc handler for id '" << msg.id << "': " << e.what();
                        }
                    }
                    else {
                        std::stringstream log_msg;
                        log_msg << "received untracked jrpc response with ";
                        if (msg.isError()) {
                            log_msg << "error '" << msg.getIfError()->what() << "'";
                        }
                        else {
                            log_msg << "result '" << *msg.getIfResult() << "'";
                        }
                        LOG(INFO) << log_msg.str();
                    }
                }

                if (_readAsyncLoopEnabled)
                    readAsync(cxn); //continue listening in an endless loop
            });

        }

        void JsonRpcUtil::callAsync(CxnHandle cxn, Message request, ResponseHandler &&handler) {
            RNR_EXPECTS(request.isRequest());
            _pending.addForId(request.id, std::move(handler));
            writeAsync(cxn, std::move(request)); //send rpc call
        }

        bool JsonRpcUtil::hasMethod(const char *name) const {
            for (auto &method : _methods) {
                if (method.name() == name)
                    return true;
            }
            return false;
        }

        void JsonRpcUtil::setReadAsyncLoopEnabled(bool val) {
            RNR_EXPECTS(isIoThread() || !hasLaunched());
            _readAsyncLoopEnabled = val;
        }

        void JsonRpcUtil::callAsyncRetryNTimes(CxnHandle cxn, Message request, uint32_t maxTries, milliseconds freq, ResponseHandler &&handler,
                                               std::function<void()> neverRespondedHandler) {

            auto stillPending = std::make_shared<bool>(true);
            uint32_t tries = 0;

            //this function keeps retrying until the provided lambda returns true
            retryAsyncEvery(freq, [this, cxn = std::move(cxn), //move all the args into the lambda
                                   stillPending, tries, maxTries,
                                   neverRespondedHandler,
                                   request = std::move(request),
                                   handler = std::move(handler)] () mutable -> bool {

                VLOG(6) << "Retry state: stillPending = " << *stillPending << ", " << tries << "/" << maxTries << " tries";
                bool keepTrying = tries < maxTries && *stillPending;

                if (keepTrying) {
                    if (tries++ == 0) {
                        //send proper tracked call at the first ry
                        callAsync(cxn, request, [handler = std::move(handler), stillPending] (CxnHandle cxn, auto response) {
                            *stillPending = false;
                            handler(cxn, std::move(response));
                        });
                    }
                    else {
                        //for every other try,just resend the message
                        writeAsync(cxn, request);
                    }
                }
                else {//stop trying
                    if (*stillPending) {
                        //clean up pending handler
                        auto _ = _pending.tryPopForId(request.id);
                        RNR_EXPECTS(_.has_value()); //this handler is supposed to exist, since *stillPending is true

                        neverRespondedHandler(); //notify callee that there was no response
                    }
                }

                return !keepTrying;
            }, /*onCancelled:*/ neverRespondedHandler);
        }

    }}
