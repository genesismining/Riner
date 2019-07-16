
#include "Pool.h"
#include "PoolEthash.h"
#include "PoolGrin.h"

namespace miner {

    /**
     * Register all Pool implementations here.
     * This is necessary so that the compiler includes the necessary code into a library
     * because only Pool is referenced by another compilation unit.
     */
    static const Pool::Registry<PoolEthashStratum> poolEthashRegistry {"PoolEthashStratum", AlgorithmEthash::getName(), "EthashStratum3"};
    static const Pool::Registry<PoolGrinStratum> poolGrinRegistry {"PoolGrinStratum", AlgorithmCuckatoo31::getName(), "EthashStratum3"};


    uint64_t Pool::createNewPoolUid() {
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

    unique_ptr<Pool> Pool::makePool(PoolConstructionArgs args, const std::string &poolImplName) {
        if (auto entry = entryWithName(poolImplName))
            return entry.value().makeFunc(std::move(args));
        return nullptr;
    }

    unique_ptr<Pool> Pool::makePool(PoolConstructionArgs args, const std::string &powType, const std::string &protocolType) {
        for (auto &entry : getEntries()) {
            bool hasMatchingName =
                    protocolType == entry.protocolType ||
                    protocolType == entry.protocolTypeAlias;

            if (hasMatchingName && entry.powType == powType)
                return entry.makeFunc(std::move(args));
        }
        return nullptr;
    }

    std::string Pool::getPoolImplNameForPowAndProtocolType(const std::string &powType, const std::string &protocolType) {
        for (auto &entry : getEntries())
            if (entry.powType == powType && entry.protocolType == protocolType)
                return entry.poolImplName;
        return ""; //no matching poolImpl found
    }

    std::string Pool::getPowTypeForPoolImplName(const std::string &poolImplName) {
        if (auto entry = entryWithName(poolImplName))
            return entry.value().powType;
        return ""; //no matching powType found
    }

    std::string Pool::getProtocolTypeForPoolImplName(const std::string &poolImplName) {
        if (auto entry = entryWithName(poolImplName))
            return entry.value().protocolType;
        return ""; //no matching protocol type found
    }

    auto Pool::entryWithName(const std::string &poolImplName) -> optional_ref<Entry> {
        for (auto &entry : getEntries())
            if (entry.poolImplName == poolImplName)
                return type_safe::opt_ref(entry);
        return nullopt;
    }

}