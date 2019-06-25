//
//

#pragma once

#include <functional>
#include <string>
#include <src/common/Chrono.h>
#include <src/common/Pointers.h>
#include <src/util/Logging.h>
#include <src/common/Assert.h>
#include <src/util/TemplateUtils.h>

namespace miner {

    enum class IOMode {
        Tcp,
        TcpSsl,
    };

    class IOConnection;
    using CxnHandle = weak_ptr<IOConnection>; //Conntection Handle that is safe to be stored by the user and reused. If the corresponding connection no longer exists, the weak_ptr catches that behavior safely. a connection is dropped when no operation is happening on it (e.g. listen)

    template<class T> using IOOnReceiveValueFunc = std::function<void(CxnHandle, const T &)>;
    using IOOnConnectedFunc = std::function<void(CxnHandle)>;
    using IOOnDisconnectedFunc = std::function<void()>;

    template<class T>
    inline void ioOnReceiveValueNoop(CxnHandle, const T &) {} //default argument for IOOnReceiveValueFunc<T>
    inline void ioOnConnectedNoop(CxnHandle) {} //default argument for IOOnConnectedFunc
    inline void ioOnDisconnectedNoop() {};

    struct IOConversionError : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    // IOTypeLayer
    // used as a base class for network io abstractions that convert to/from certain types
    // example:
    //
    // std::string <-> nl::json
    //                 nl::json <-> JsonRpc
    //
    // could be implemented as:
    //
    // class StringLayer {...};
    // class JsonLayer    : public IOTypeLayer<nl::json, StringLayer> {...};
    // class JsonRpcLayer : public IOTypeLayer<JsonRpc, JsonLayer> {...};
    //
    // in this example StringLayer implements all the actual IO logic (which runs
    // on strings). JsonLayer and JsonRpcLayer only provide conversion
    // functions while still being able to expose the same functionality as
    // StringLayer. In the end you only end up instantiating JsonRpcLayer, which
    // internally instantiates the others in order to provide JsonRpc related
    // functionality.
    //
    // this solution was chosen to make adding new generic features to all the
    // IO type abstractions less tedious, they now only have to be added to the
    // bottom most layer and the IOTypeLayer template.
    //
    // If you want to implement a new layer, just subclass from the IOTypeLayer
    // you want to build on top of as shown above, and override these functions:
    //    T           convertIncoming(LayerBelowT)
    //    LayerBelowT convertOutgoing(T)
    // in which you have to interpret the incoming messages from the layer below
    // in terms of your layer's type, and translate them back upon sending.

    template<class T, class LayerBelow>
    class IOTypeLayer {
    public:
        static_assert(has_value_type_v<LayerBelow>, "LayerBelow must define a LayerBelow::value_type");

        using value_type = T;
        using LayerBelowT = typename LayerBelow::value_type;
        using OnReceiveValueFunc = IOOnReceiveValueFunc<T>;

        using ModifierFunc = std::function<void(T &t)>;

        LayerBelow &layerBelow() {
            return _layerBelow;
        }

        //takes callback that is called whenever an incoming message was
        //successfully converted to a T (e.g. if no exceptions were thrown) and
        //also passes an IOConnection that can be used within that callback to send responses
        IOTypeLayer(IOMode mode)
        : _layerBelow(mode) {
        }

        virtual T convertIncoming(LayerBelowT) = 0; //e.g. json(string);
        virtual LayerBelowT convertOutgoing(T) = 0; //e.g. string(json);

        virtual ~IOTypeLayer() = default;

        //let the user define a function that modifies an incoming T before it is further processed
        void setIncomingModifier(ModifierFunc &&func) {
            checkNotLaunchedOrOnIOThread();
            _incomingModifier = std::move(func);
        }

        //let the user define a function that modifies an outgoing T before it is sent
        void setOutgoingModifier(ModifierFunc &&func) {
            checkNotLaunchedOrOnIOThread();
            _outgoingModifier = std::move(func);
        }

        void readAsync(CxnHandle cxn) {
            _layerBelow.readAsync(cxn);
        }

        void setOnReceive(OnReceiveValueFunc &&onRecv) {
            checkNotLaunchedOrOnIOThread();
            _layerBelow.setOnReceive([this, onRecv = std::move(onRecv)] (CxnHandle cxn, LayerBelowT lbt) {
                try {
                    onRecv(cxn, processIncoming(std::move(lbt)));
                }
                catch(const IOConversionError &e) {
                    LOG(WARNING) << "IO conversion failed on receive in IOTypeLayer for "
                    << typeid(value_type).name() << ": " << e.what();
                }
            });
        };

        void writeAsync(CxnHandle cxn, T t) {
            //forward this call to the layer below, but process t so it is compatible with that layer
            try {
                _layerBelow.writeAsync(cxn, processOutgoing(std::move(t)));
            }
            catch(const IOConversionError &e) {
                LOG(WARNING) << "IO conversion failed on asyncWrite in IOTypeLayer for "
                             << typeid(value_type).name() << ": " << e.what();
            }
        }

        template<class Func>
        void postAsync(Func &&func) {
            _layerBelow.postAsync(std::forward<Func>(func));
        }

        //asynchronously call pred every retryInterval milliseconds until it returns true for the first time
        //first call happens right away
        //just like postAsync(...) this function is just a utility to make use of asio's thread pool and does
        //not actually write anything to a connection
        void retryAsyncEvery(milliseconds retryInterval, std::function<bool()> &&pred) {
            _layerBelow.retryAsyncEvery(retryInterval, std::move(pred));
        }

        void launchClient(std::string host, uint16_t port, IOOnConnectedFunc onCxn = ioOnConnectedNoop, IOOnDisconnectedFunc onDc = ioOnDisconnectedNoop) {
            _layerBelow.launchClient(std::move(host), port, std::move(onCxn), std::move(onDc));
        }

        void launchClientAutoReconnect(std::string host, uint16_t port, IOOnConnectedFunc onCxn = ioOnConnectedNoop, IOOnDisconnectedFunc onDc = ioOnDisconnectedNoop) {
            _layerBelow.launchClientAutoReconnect(std::move(host), port, std::move(onCxn), std::move(onDc));
        }

        void launchServer(uint16_t port, IOOnConnectedFunc onCxn = ioOnConnectedNoop, IOOnDisconnectedFunc onDc = ioOnDisconnectedNoop) {
            _layerBelow.launchServer(port, std::move(onCxn));
        }

        bool hasLaunched() {
            return layerBelow().hasLaunched();
        }

        bool isIoThread() {
            return layerBelow().isIoThread();
        }

    private:
        T processIncoming(LayerBelowT lbt) {
            //apply all necessary changes to the incoming data so that it can be
            //processed as a T
            auto t = convertIncoming(std::move(lbt));
            if (_incomingModifier) //user may have defined a custom modifier func
                _incomingModifier(t);
            return t;
        }

        LayerBelowT processOutgoing(T t) {
            //apply all necessary changes to the incoming T so that it can be
            //interpreted by the layer below
            if (_outgoingModifier) //user may have defined a custom modifier func
                _outgoingModifier(t);
            auto lbt = convertOutgoing(std::move(t));
            return lbt;
        }

        void checkNotLaunchedOrOnIOThread() {
            MI_EXPECTS(!hasLaunched() || isIoThread()); //mutating functions may only be called before calling launch, or from the IO Thread
        }

        ModifierFunc _incomingModifier;
        ModifierFunc _outgoingModifier;
        LayerBelow _layerBelow;
    };

}