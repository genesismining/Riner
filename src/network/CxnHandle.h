//
//

#pragma once

namespace miner {

    class IOConnection;
    class BaseIO;

    /**
     * Connection Handle that is safe to be stored by the user and reused.
     * If the corresponding connection no longer exists, the weak_ptr deals with that behavior safely.
     * A connection is dropped when no operation is happening on it (e.g. no async read/writes are queued)
     *
     * Implementation wise a CxnHandle is just a glorified weak_ptr that can only be locked by BaseIO
     */

    class CxnHandle {
        weak_ptr<IOConnection> _weakPtr;
        shared_ptr<IOConnection> lock(BaseIO &);
    public:
        friend class BaseIO; //can only be locked by baseIO

        CxnHandle() = default;

        explicit CxnHandle(weak_ptr<IOConnection> w)
        : _weakPtr(std::move(w)) {
        };

    };

}