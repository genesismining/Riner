//
//

#include "DeviceRecords.h"

namespace miner {

    void DeviceRecords::reportAmtTraversedNonces(uint64_t inAmt) {
        _traversedNonces.addRecord(inAmt);
    }

    void DeviceRecords::reportFailedShareVerification() {
        _failedShareVerifications.addRecord(1);
    }

    uint64_t DeviceRecords::getVerificationDifficulty() const {
        return 1;
    }

    void DeviceRecords::merge(const DeviceRecords &) {

    }

    const DeviceRecords::Entry &DeviceRecords::getTraversedNonces() const {
        return _traversedNonces;
    }

    const DeviceRecords::Entry &DeviceRecords::getFailedShareVerifications() const {
        return _failedShareVerifications;
    }

}