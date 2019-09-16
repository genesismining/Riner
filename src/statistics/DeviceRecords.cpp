//
//

#include "DeviceRecords.h"

namespace miner {

    void DeviceRecords::reportAmtTraversedNonces(uint64_t inAmt) {
        _node.lockedForEach([inAmt] (Data &data) {
            data.traversedNonces.addRecord(inAmt);
        });
    }

    void DeviceRecords::reportShare(double difficulty, bool valid) {
        _node.lockedForEach([=] (Data &data) {
            if (valid) {
                data.validShares.addRecord(difficulty);
            }
            else {
                data.invalidShares.addRecord(difficulty);
            }
        });
    }

    DeviceRecords::Data DeviceRecords::read() const {
        return _node.getValue();
    }

    DeviceRecords::DeviceRecords(DeviceRecords &parent) {
        parent._node.addListener(_node);
    }

}