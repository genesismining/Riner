//
//

#pragma once

#include <functional>
#include <string>
#include <src/common/Chrono.h>
#include <src/common/Pointers.h>
#include <src/util/Logging.h>
#include <src/util/TemplateUtils.h>

namespace miner {

    enum class IOMode {
        Tcp,
        TcpSsl,
    };

    class IOConnection;
    using CxnHandle = weak_ptr<IOConnection>; //Conntection Handle

    template<class T> using IOOnReceiveValueFunc = std::function<void(CxnHandle, const T &)>;
    using IOOnConnectedFunc = std::function<void(CxnHandle)>;

    template<class T>
    void ioOnReceiveValueNoop(CxnHandle, const T &) {} //default argument for IOOnReceiveValueFunc<T>
    void ioOnConnectedNoop(CxnHandle) {} //default argument for IOOnConnectedFunc

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

        //let the user define a function that gets called with a mutable T &,
        //and gets called whenever a new object is received.
        void setIncomingModifier(ModifierFunc &&func) {
            _incomingModifier = std::move(func);
        }

        //let the user define a function that gets called with a mutable T &,
        //and gets called whenever a new object is sent.
        void setOutgoingModifier(ModifierFunc &&func) {
            _outgoingModifier = std::move(func);
        }

        void asyncRead(CxnHandle cxn) {
            _layerBelow.asyncRead(cxn);
        }

        void setOnReceive(OnReceiveValueFunc &&onRecv) {
            _layerBelow.setOnReceive([this, onRecv = std::move(onRecv)] (CxnHandle cxn, const LayerBelowT &lbt) {
                try {
                    onRecv(processIncoming(onRecv));
                }
                catch(const IOConversionError &e) {
                    LOG(WARNING) << "IO conversion failed on receive in IOTypeLayer for "
                    << typeid(value_type).name() << ": " << e.what();
                }
            });
        };

        void asyncWrite(CxnHandle cxn, T t) {
            //forward this call to the layer below, but process t so it is compatible with that layer
            try {
                _layerBelow.asyncWrite(cxn, processOutgoing(std::move(t)));
            }
            catch(const IOConversionError &e) {
                LOG(WARNING) << "IO conversion failed on asyncWrite in IOTypeLayer for "
                             << typeid(value_type).name() << ": " << e.what();
            }
        }

        void asyncWriteRetryEvery(T t, milliseconds retryInterval, CxnHandle cxn, std::function<bool()> &&pred) {
            //forward this call to the layer below, but process t so it is compatible with that layer
            try {
                _layerBelow.asyncRetryEvery(processOutgoing(std::move(t)), retryInterval, cxn, std::move(pred));
            }
            catch(const IOConversionError &e) {
                LOG(WARNING) << "IO conversion failed on asyncWriteRetryEvery in IOTypeLayer for "
                             << typeid(value_type).name() << ": " << e.what();
            }
        }

        void launchClient(std::string host, uint16_t port, IOOnConnectedFunc onCxn = ioOnConnectedNoop) {
            _layerBelow.launchClient(std::move(host), port, std::move(onCxn));
        }

        void launchServer(uint16_t port, IOOnConnectedFunc onCxn = ioOnConnectedNoop) {
            _layerBelow.launchServer(port, std::move(onCxn));
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

        LayerBelowT processOutgoing(T lbt) {
            //apply all necessary changes to the incoming T so that it can be
            //interpreted by the layer below
            auto t = convertOutgoing(std::move(lbt));
            if (_outgoingModifier) //user may have defined a custom modifier func
                _outgoingModifier(t);
            return t;
        }

        ModifierFunc _incomingModifier;
        ModifierFunc _outgoingModifier;
        LayerBelow _layerBelow;
    };

}