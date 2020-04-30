//
//

#include "DeviceRecords.h"

namespace riner {

    void DeviceRecords::reportScannedNoncesAmount(uint64_t inAmt) {
        _node.lockedForEach([inAmt] (Data &data) {
            data.scannedNonces.addRecord(inAmt);
        });
    }

    void DeviceRecords::reportWorkUnit(double difficulty, bool valid) {
        _node.lockedForEach([=] (Data &data) {
            if (valid) {
                data.validWorkUnits.addRecord(difficulty);
            }
            else {
                data.invalidWorkUnits.addRecord(difficulty);
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