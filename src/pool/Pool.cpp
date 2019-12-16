
#include "Pool.h"
#include "PoolEthash.h"
#include "PoolGrin.h"

namespace miner {

    /**
     * Register all Pool implementations here.
     * This is necessary so that the compiler includes the necessary code into a library
     * because only Pool is referenced by another compilation unit.
     */
    static const Pool::Registry<PoolEthashStratum> registryPoolEthashStratum {"EthashStratum2", HasPowTypeEthash::getPowType(), "stratum2"};
    static const Pool::Registry<PoolGrinStratum>   registryPoolGrinStratum   {"Cuckatoo31Stratum", HasPowTypeCuckatoo<31>::getPowType(), "GrinStratum", "stratum"};


    std::atomic_uint64_t Pool::poolCounter {0};

    Pool::Pool(PoolConstructionArgs args)
            : constructionArgs(std::move(args)) {
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

    void StillAliveTrackable::declareDead() {
        latestDeclaredDeadTime = clock::now();
        onDeclaredDead();
    }

    StillAliveTrackable::clock::time_point
    StillAliveTrackable::getLatestDeclaredDeadTime() {
        return latestDeclaredDeadTime;
    }

    shared_ptr<Pool> Pool::makePool(const PoolConstructionArgs &args, const std::string &poolImplName) {
        if (auto entry = entryWithName(poolImplName))
            return entry->makeFunc(args);
        return nullptr;
    }

    std::string Pool::getPoolImplNameForPowAndProtocolType(const std::string &powType, const std::string &protocolType) {
        for (const auto &entry : getEntries()) {
            bool hasMatchingName =
                    protocolType == entry.protocolType ||
                    (!entry.protocolTypeAlias.empty() && protocolType == entry.protocolTypeAlias);

            if (hasMatchingName && entry.powType == powType)
                return entry.poolImplName;
        }
        return ""; //no matching poolImpl found
    }

    std::string Pool::getPowTypeForPoolImplName(const std::string &poolImplName) {
        if (auto entry = entryWithName(poolImplName))
            return entry->powType;
        return ""; //no matching powType found
    }

    std::string Pool::getProtocolTypeForPoolImplName(const std::string &poolImplName) {
        if (auto entry = entryWithName(poolImplName))
            return entry->protocolType;
        return ""; //no matching protocol type found
    }

    bool Pool::hasPowType(const std::string &powType) {
        for (const auto &entry : getEntries()) {
            if (entry.powType == powType) {
                return true;
            }
        }
        return false;
    }

    bool Pool::hasProtocolType(const std::string &protocolType) {
        for (const auto &entry : getEntries()) {
            if (entry.protocolType == protocolType ||
                (!entry.protocolTypeAlias.empty() && protocolType == entry.protocolTypeAlias)) {
                return true;
            }
        }
        return false;
    }

    optional_ref<Pool::Entry> Pool::entryWithName(const std::string &poolImplName) {
        for (auto &entry : getEntries())
            if (entry.poolImplName == poolImplName)
                return entry;
        return {};
    }

}