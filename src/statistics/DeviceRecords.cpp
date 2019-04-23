//
//

#include "DeviceRecords.h"

namespace miner {

    void DeviceRecords::reportAmtTraversedNonces(uint64_t inAmt) {
        _node.lockedForEach([inAmt] (Data &data) {
            data.traversedNonces.addRecord(inAmt);
        });
    }

    void DeviceRecords::reportFailedShareVerification() {
        _node.lockedForEach([] (Data &data) {
            data.failedShareVerifications.addRecord(1);
        });
    }

    uint64_t DeviceRecords::getVerificationDifficulty() const {
        return 0;
    }

    DeviceRecords::Data DeviceRecords::read() const {
        return _node.getValue();
    }

    DeviceRecords::DeviceRecords(DeviceRecords &parent) {
        parent._node.addListener(_node);
    }

}