
#include "Pool.h"
#include "PoolEthash.h"
#include "PoolGrin.h"

namespace riner {

    Pool::Pool(PoolConstructionArgs args)
            : constructionArgs(std::move(args)) {
    }

    uint64_t Pool::generateUid() {
        static std::atomic<uint64_t> nextUid = {1};
        return nextUid++;
    }

    void StillAliveTrackable::onStillAlive() {
        lastKnownAliveTime = clock::now();
    };

    StillAliveTrackable::clock::time_point
    StillAliveTrackable::getLastKnownAliveTime() {
        return lastKnownAliveTime;
    }

    StillAliveTrackable::clock::time_point
    StillAliveTrackable::getObjectCreationTime() {
        return objectCreationTime;
    }

    void StillAliveTrackable::setLastKnownAliveTime(clock::time_point time) {
        lastKnownAliveTime = time;
    }

    void StillAliveTrackable::setDead(bool dead) {
        bool changed = dead != _isDead;
        _isDead = dead;
        if (_isDead && changed) {
            latestDeclaredDeadTime = clock::now();
            onDeclaredDead();
        }
    }

    bool StillAliveTrackable::isDead() const {
        return _isDead;
    }

    StillAliveTrackable::clock::time_point
    StillAliveTrackable::getLatestDeclaredDeadTime() {
        return latestDeclaredDeadTime;
    }

}