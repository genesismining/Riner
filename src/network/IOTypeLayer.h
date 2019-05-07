//
//

#pragma once

#include <functional>
#include <src/common/Chrono.h>
#include <src/util/TemplateUtils.h>

namespace miner {

    enum class IOMode {
        Tcp,
        TcpSsl,
    };

    class IOConnection;

    template<class T> using IOOnReceiveValueFunc = std::function<void(const T &, IOConnection &)>;

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
        IOTypeLayer(IOMode mode, OnReceiveValueFunc &&onRecv)
        : _layerBelow(mode, [this, onRecv = std::move(onRecv)] (const LayerBelowT &lbt, IOConnection &conn) {
            onRecv(processIncoming(lbt), conn);
        }) {
        }

        virtual T convertIncoming(LayerBelowT) = 0; //e.g. json(string);
        virtual LayerBelowT convertOutgoing(T) = 0; //e.g. string(json);

        virtual ~IOTypeLayer() = default;

        //let the user define a function that gets called with a mutable T &,
        //and gets called whenever a new object is received.
        void setIncomingModifier(ModifierFunc &&func) {
            _incomingModifier(std::move(func));
        }

        //let the user define a function that gets called with a mutable T &,
        //and gets called whenever a new object is sent.
        void setOutgoingModifier(ModifierFunc &&func) {
            _outgoingModifier(std::move(func));
        }

        void asyncRead(IOConnection &conn) {
            _layerBelow.asyncRead(conn);
        }

        void asyncWrite(T t, IOConnection &conn) {
            //forward this call to the layer below, but process t so it is compatible with that layer
            _layerBelow.asyncWrite(processOutgoing(std::move(t)), conn);
        }

        void asyncWriteRetryEvery(T t, milliseconds retryInterval, IOConnection &conn, std::function<bool()> &&pred) {
            //forward this call to the layer below, but process t so it is compatible with that layer
            _layerBelow.asyncRetryEvery(processOutgoing(std::move(t)), retryInterval, conn, std::move(pred));
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