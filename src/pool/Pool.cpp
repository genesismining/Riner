
#include "Pool.h"

namespace miner {

    uint64_t WorkProvider::createNewPoolUid() {
        static std::atomic<uint64_t> uid = {1};
        return uid.fetch_add(1);
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