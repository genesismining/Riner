
#include "Pool.h"

namespace miner {

    uint64_t WorkProvider::createNewPoolUid() {
        static uint64_t uid = 0;
        ++uid;
        return uid;
    }

    void StillAliveTrackable::onStillAlive() {
        lastKnownAliveTime = clock::now();
    };

    StillAliveTrackable::clock::time_point
    StillAliveTrackable::getLastKnownAliveTime() {
        return lastKnownAliveTime;
    }

    void StillAliveTrackable::setLastKnownAliveTime(clock::time_point time) {
        lastKnownAliveTime = time;
    }

}