
#include "Pool.h"
#include "PoolEthash.h"
#include "PoolGrin.h"

namespace riner {

    Pool::Pool(PoolConstructionArgs args)
            : constructionArgs(std::move(args)) {
    }

    void Pool::postInit(std::shared_ptr<Pool> w, const std::string &poolImplName, const std::string &powType) {
        w->_this = w;
        w->_poolImplName = poolImplName;
        w->_powType = powType;
    }

    void Pool::setConnected(bool connected) {
        if (connected != _connected && onStateChange) {
            VLOG(5) << "changed connected flag to " << connected;
            if (connected) {
                clearJobs();
                records.resetInterval();
                _connected = true;
            }
            else {
                _connected = false;
                records.resetInterval();
            }
            onStateChange->notify_all();
        }
    }

    void Pool::setDisabled(bool disabled) {
        if (disabled != _disabled.exchange(disabled) && onStateChange) {
            onStateChange->notify_all();
        }
    }

    void Pool::setActive(bool active) {
        if (active != _active.exchange(active) && onStateChange) {
            onDeclaredDead();
        }
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
