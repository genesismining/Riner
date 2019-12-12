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
#include <src/network/SslDesc.h>
#include <src/network/CxnHandle.h>

namespace miner {

    enum class IOMode {
        Tcp,
    };

    //function declarations that are used in IO related classes
    template<class T> using IOOnReceiveValueFunc = std::function<void(CxnHandle, const T &)>;
    using IOOnConnectedFunc = std::function<void(CxnHandle)>;
    using IOOnDisconnectedFunc = std::function<void()>;

    //noop implementations of IO related functions, used for default arguments
    template<class T>
    inline void ioOnReceiveValueNoop(CxnHandle, const T &) {} //default argument for IOOnReceiveValueFunc<T>
    inline void ioOnConnectedNoop(CxnHandle) {} //default argument for IOOnConnectedFunc
    inline void ioOnDisconnectedNoop() {};

    /**
     * An IOConversionError should be thrown whenever an IOTypeLayer implementation fails to convert its argument in either its convertIncoming() or convertOutgoing() method.
     * For more details see std::runtime_error
     */
    struct IOConversionError : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    /**
     * `IOTypeLayer`
     * is used as a base class for network io abstractions that convert to/from certain types
     *
     * ```
     *    example:
     *    std::string <-> nl::json              //one IOTypeLayer
     *                    nl::json <-> JsonRpc  //another IOTypeLayer
     * ```
     *
     * In this example, the incoming byte stream is first interpreted as a `std::string`, once that
     * was successful, the next layer tries to parse the `std::string` as a `nl::json`.
     * after that, the next layer tries to parse the `nl::json` as a `JsonRpc::Message`.
     * The core functionality is only implemented based on `std::strings`, and the
     * json and json-rpc layers only add 2 functions each for forward and backward
     * conversions. Adding a new layer is as easy as providing 2 such conversion functions.
     * The resulting `IOTypeLayer` subclass still provides the same public interface
     * as the implementation on the `std::string` level.
     *
     * could be implemented as:
     * ```cpp
     *    class LineIO {...}; //std::string layer
     *    class JsonIO       : public IOTypeLayer<nl::json        , LineIO> {...};
     *    class JsonRpcIO    : public IOTypeLayer<JsonRpc::Message, JsonIO> {...};
     * ```
     *
     * in this example `LineIO` implements all the actual IO logic (which runs
     * on `std::string`s). `JsonIO` and `JsonRpcIO` only provide conversion
     * functions while still being able to expose the same functionality as
     * `LineIO`. In the end you only end up instantiating `JsonRpcIO`, which
     * internally instantiates all the others in order to provide json-rpc related
     * functionality.
     *
     * this solution was chosen to make adding new generic features to all the
     * IO type-abstractions less tedious. They now only have to be added to the
     * bottom most layer and the `IOTypeLayer` template.
     *
     * If you want to implement a new layer, just subclass from the `IOTypeLayer`
     * you want to build on top of as shown above.
     * Then implement a destructor that calls `stopIOThread()` to make sure no
     * io thread handlers are executing while your class destroys its members.
     * Finally override the following functions:
     * ```cpp
     *    T           convertIncoming(LayerBelowT)
     *    LayerBelowT convertOutgoing(T)
     * ```
     * in which you have to interpret the incoming messages from the layer below
     * in terms of your layer's type and translate them back upon sending.
     *
     */

    template<class T, class LayerBelow>
    class IOTypeLayer {
    public:
        static_assert(has_value_type_v<LayerBelow>, "LayerBelow must define a LayerBelow::value_type");

        template<class U, class V>
        friend class IOTypeLayer;

        using value_type = T;
        using LayerBelowT = typename LayerBelow::value_type;
        using OnReceiveValueFunc = IOOnReceiveValueFunc<T>;

        /**
         * Function type passed to `setIncomingModifier()` and `setOutgoingModifier()`
         */
        using ModifierFunc = std::function<void(T &t)>;

        /**
         * obtain the layer below, e.g. for setting modifier functions via `setIncomingModifier()` or `setOutgoingModifier()`
         * for that specific layer's type. Usually chained to obtain the layer you want (`io.layerBelow().layerBelow()...`)
         * @return the instance of `LayerBelow` that this layer builds upon
         */
        LayerBelow &layerBelow() {
            return _layerBelow;
        }

        /**
         * Constructor of the `IOTypeLayer` does not establish any connection yet. Use `launchClient*()` or `launchServer()` functions
         * @param mode not implemented yet
         */
        IOTypeLayer(IOMode mode)
                : _layerBelow(mode) {
        }

        /**
         * converts the incoming type from the layer below to the current layer's type `T`.
         * This function may throw an `IOConversionError` if the conversion failed.
         * @return successfully converted instance of `T`
         */
        virtual T convertIncoming(LayerBelowT) = 0; //e.g. json(string);

        /**
         * converts the outgoing type from this layer's `T` to the type of the layer below.
         * This function may throw an `IOConversionError` if the conversion failed.
         * @return successfully converted instance of `LayerBelowT`
         */
        virtual LayerBelowT convertOutgoing(T) = 0; //e.g. string(json);

        virtual ~IOTypeLayer() {
            if (ioThreadRunning()) {
                LOG(ERROR) << "IOTypeLayer subclass must call 'stopIOThread()' at the start of its destructor to prevent the IO Thread from accessing destroyed resources.";
            }
            MI_ENSURES(!ioThreadRunning());
        };

        /**
         * provide a `ModifierFunc` which can define custom modifications that will be applied to any incoming `T` object
         * before it is exposed in any other way. This is useful e.g. if a specific pool protocol is implemented that
         * has some minor quirks on the way it expects Json Rpcs to behave.
         * @param func function that lets the user modify every incoming `T` before it gets exposed otherwise
         */
        void setIncomingModifier(ModifierFunc &&func) {
            checkNotLaunchedOrOnIOThread();
            _incomingModifier = std::move(func);
        }

        /**
         * provide a `ModifierFunc` that can define custom modifications that will be applied to any outgoing `T` object
         * before it is converted to a `LayerBelowT`. This is useful e.g. if a specific pool protocol is implemented that
         * has some minor quirks on the way it expects Json Rpcs to behave.
         * @param func function that lets the user modify every outgoing `T` before it is sent
         */
        void setOutgoingModifier(ModifierFunc &&func) {
            checkNotLaunchedOrOnIOThread();
            _outgoingModifier = std::move(func);
        }

        /**
         * use this function to enable ssl (configurable via `desc`) for the connections that will be opened by this IO object.
         * This function must be called before any launch* functions.
         */
        void enableSsl(const SslDesc &desc) {
            checkNotLaunchedOrOnIOThread();
            _layerBelow.enableSsl(desc);
        }

        /**
         * Enqueues an async read operation on the provided `cxn` and keeps that `cxn` alive (if it hasn't disconnected yet).
         * Once a read was queued and bytes were received that successfully went through the chain of `IOTypeLayers`, the
         * optional `OnReceiveValueFunc` provided in `setOnReceive()` gets called with the resulting `T` instance. Otherwise
         * (if no func was provided) the behavior is identical, but the default `OnReceiveValueFunc` (=noop) gets called.
         * @param cxn the connection handle to queue a read operation on
         */
        void readAsync(CxnHandle cxn) {
            _layerBelow.readAsync(cxn);
        }

        /**
         * Provide a function that gets called whenever an incoming message was
         * successfully converted to a `T` (e.g. if no exceptions were thrown) and
         * also passes an `CxnHandle` that can be used within that callback to send responses/queue reads
         * @param onRecv the function that is called upon receiving a `T`
         */
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

        /**
         * Enqueues an async write operation on the provided `cxn` and keeps that `cxn` alive (if it hasn't disconnected yet).
         * The t object will get converted down to the base layer on the calling thread, then the actual async write is
         * queued. For more details see boost::asio's `async_write`.
         * @param cxn the connection handle to queue a write operation on
         * @param t the object that gets first converted to the bottom most layer and then async-sent over the `cxn`
         */
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

        /**
         * Enqueue an arbitrary function to run on the IO thread. For more details see boost::asio's `asio::post`.
         * @tparam Func functor with a `void operator()` that takes no arguments
         * @param func the function which is going to be executed on the io thread.
         */
        template<class Func>
        void postAsync(Func &&func) {
            _layerBelow.postAsync(std::forward<Func>(func));
        }

        /**
         * call an arbitrary predicate function every `retryInterval` milliseconds until it succeeds (returns `true`) for
         * the first time.
         * @param retryInterval the time duration to wait between the individual tries
         * @param pred the function that should be called repeatedly until it returns `true` for the first time
         * @param onCancelled the function that should be called if the retrires had to be cancelled (e.g. if the IOTypeLayer got destroyed, or all handlers got aborted due to disconnectAll call). This function is guaranteed to be called after the ioThread is no longer executing (=joined). The function is called from within a user's thread that interacts with this io object (e.g. via its dtor).
         */
        void retryAsyncEvery(milliseconds retryInterval, std::function<bool()> &&pred, std::function<void()> onCancelled) {
            _layerBelow.retryAsyncEvery(retryInterval, std::move(pred), std::move(onCancelled));
        }

        /**
         * `launchClient()` opens a single connection to the given host, port.
         * @param host host name or ip e.g. "localhost" or "127.0.0.1" to connect to
         * @param port port to connect to
         * @param onCxn function that gets called on the io thread when a successful connection was established. A `CxnHandle` is provided
         * as a parameter to enqueue further operations on that connection.
         * @param onDc function that gets called (not necessarily on the io thread!) when the connection was either closed on the other side, lost, or closed on this side
         * by no longer enqueueing any async read/write operations on the `CxnHandle` after it was successfully connected.
         */
        void launchClient(std::string host, uint16_t port, IOOnConnectedFunc onCxn = ioOnConnectedNoop, IOOnDisconnectedFunc onDc = ioOnDisconnectedNoop) {
            _layerBelow.launchClient(std::move(host), port, std::move(onCxn), std::move(onDc));
        }

        /**
         * Same as `launchClient()`, but it tries to reconnect whenever a connection was lost. The `onDc` and later `onCxn` callbacks also get called whenever that happens.
         * @param onCxn function that gets called on the io thread whenever a successful connection was established.
         * @param onDc function that gets called (not necessarily on the io thread!) whenever a connection was closed from the other side, lost, or closed on this side.
         */
        void launchClientAutoReconnect(std::string host, uint16_t port, IOOnConnectedFunc onCxn = ioOnConnectedNoop, IOOnDisconnectedFunc onDc = ioOnDisconnectedNoop) {
            _layerBelow.launchClientAutoReconnect(std::move(host), port, std::move(onCxn), std::move(onDc));
        }

        /**
         * `launchServer()` listens on the provided port for incoming connections. For every connection, a `CxnHandle` is created and the `onCxn` callback is called with it.
         * @param port the port to listen on for connections
         * @param onCxn the function that is called on the IO thread on every successfully established connection
         * @param onDc function that gets called (not necessarily on the io thread!) whenever a connection was closed from the other side, lost, or closed on this side.
         */
        void launchServer(uint16_t port, IOOnConnectedFunc onCxn = ioOnConnectedNoop, IOOnDisconnectedFunc onDc = ioOnDisconnectedNoop) {
            _layerBelow.launchServer(port, std::move(onCxn), std::move(onDc));
        }

        bool hasLaunched() {
            return layerBelow().hasLaunched();
        }

        /**
         * check whether the calling thread is the io thread. this function can be called from any thread
         * @return `true` if the calling thread is the io thread owned by this object, `false` otherwise
         */
        bool isIoThread() {
            return layerBelow().isIoThread();
        }

        void disconnectAll() {
            return layerBelow().disconnectAll();
        }

    protected:
        /**
         * call this function in the dtor of your `IOTypeLayer` subclasses.
         * You will get a runtime error message to remind you of this, in case you forget.
         * Stopping the io thread that early is necessary to ensure the io thread does not accidentially access
         * members that have already been destroyed by the dtor of your `IOTypeLayer` subclass.
         */
        void stopIOThread() { //blocks until io thread has joined
            _layerBelow.stopIOThread(); //calls BaseIO::stopIOThread();
        }

    private:

        /**
         * returns whether the io thread's dtor wasn't called yet.
         * If this function returns `true` you should assume that handlers are being executed that access the layer's state
         * @return whether the io thread's dtor hasn't finished yet, and thus the io thread has not yet joined
         */
        bool ioThreadRunning() const {
            return _layerBelow.ioThreadRunning();
        }

        /**
         * apply all necessary changes to the incoming data so that it can be
         * processed as a `T`
         * @param lbt received data in the type of the layer below
         * @return data converted to instance of `T`
         */
        T processIncoming(LayerBelowT lbt) {
            auto t = convertIncoming(std::move(lbt));
            if (_incomingModifier) //user may have defined a custom modifier func
                _incomingModifier(t);
            return t;
        }

        /**
         * apply all necessary changes to the incoming `T` so that it can be
         * interpreted by the layer below
         * @param t that is about to be sent
         * @return t in the type of the layer below
         */
        LayerBelowT processOutgoing(T t) {
            if (_outgoingModifier) //user may have defined a custom modifier func
                _outgoingModifier(t);
            auto lbt = convertOutgoing(std::move(t));
            return lbt;
        }

        /**
         * shorthand for ensuring that it is ok to mutate the layer's state
         */
        void checkNotLaunchedOrOnIOThread() {
            MI_EXPECTS(!hasLaunched() || isIoThread()); //mutating functions may only be called before calling launch, or from the IO Thread
        }

        ModifierFunc _incomingModifier;
        ModifierFunc _outgoingModifier;
        LayerBelow _layerBelow;
    };

}