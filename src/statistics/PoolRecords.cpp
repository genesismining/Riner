//
//

#include "PoolRecords.h"

namespace riner {

    PoolRecords::Data PoolRecords::read() const {
        return _node.getValue();
    }

    void PoolRecords::addListener(PoolRecords &parent) {
        _node.addListener(parent._node);
    }

    void PoolRecords::reportShare(double difficulty, bool isAccepted, bool isDuplicate) {
        auto now = clock::now();
        _node.lockedForEach([=] (Data &d) {

            if (isAccepted) {
                d.acceptedShares.addRecord(difficulty, now);
            }
            else if (isDuplicate) {
                d.duplicateShares.addRecord(difficulty, now);
            }
            else {
                d.rejectedShares.addRecord(difficulty, now);
            }

        });
    }

    void PoolRecords::resetInterval() {
        _node.lockedApply([](Data &data) {
            clock::time_point time = clock::now();
            data.acceptedShares.getAndReset(time);
            data.rejectedShares.getAndReset(time);
            data.duplicateShares.getAndReset(time);
        });
    }

    clock::duration PoolRecords::Data::connectionDuration() const {
        return acceptedShares.interval.getElapsedTime(clock::now());
    }

}