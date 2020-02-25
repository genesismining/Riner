
#include "BaseIO.h"

namespace miner {

    shared_ptr<IOConnection> CxnHandle::lock(BaseIO &baseIO) {

        if (auto cxn = _weakPtr.lock()) {
            //check if lock is called with the correct baseIO instance
            if (cxn->getAssociatedBaseIOUid() == baseIO.getUid()) {
                return cxn;
            }
        }
        return nullptr;
    };


    CxnHandle::CxnHandle(weak_ptr<IOConnection> movedArg)
    : _weakPtr(std::move(movedArg))
    , ioConnectionUidCopy(_weakPtr.lock()->getConnectionUid()) {
    };

}