//
//

#include "PoolRecords.h"

namespace miner {

    PoolRecords::Data PoolRecords::read() const {
        return _node.getValue();
    }

    PoolRecords::PoolRecords(PoolRecords &parent) {
        _node.addListener(parent._node);
    }

    void PoolRecords::reportShare(bool isAccepted, bool isDuplicate) {
        auto now = clock::now();
        _node.lockedForEach([=] (Data &d) {

            if (isAccepted) {

                d.acceptedShares.addRecord(1, now);

                if (isDuplicate) {
                    d.duplicateShares.addRecord(1, now);
                }

            }
            else {
                d.rejectedShares.addRecord(1, now);
            }
        });
    }

}