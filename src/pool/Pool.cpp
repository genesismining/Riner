
#include "Pool.h"
#include "PoolEthash.h"
#include "PoolGrin.h"

namespace miner {

    /**
     * Register all Pool implementations here.
     * This is necessary so that the compiler includes the necessary code into a library
     * because only Pool is referenced by another compilation unit.
     */
    static const Pool::Registry<PoolEthashStratum> poolEthashRegistry {"PoolEthashStratum", AlgorithmEthash::getName(), kStratumTcp};
    static const Pool::Registry<PoolGrinStratum> poolGrinRegistry {"PoolGrinStratum", AlgorithmCuckatoo31::getName(), kStratumTcp};


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

    unique_ptr<Pool> Pool::makePool(PoolConstructionArgs args, const std::string &implName) {
        if (auto entry = entryWithName(implName))
            return entry.value().makeFunc(std::move(args));
        return nullptr;
    }

    unique_ptr<Pool> Pool::makePool(PoolConstructionArgs args, const std::string &algoName, ProtoEnum protoEnum) {
        for (auto &entry : getEntries())
            if (entry.algoName == algoName && entry.protoEnum == protoEnum)
                return entry.makeFunc(std::move(args));
        return nullptr;
    }

    std::string Pool::getImplNameForAlgoTypeAndProtocol(const std::string &algoName, ProtoEnum protoEnum) {
        for (auto &entry : getEntries())
            if (entry.algoName == algoName && entry.protoEnum == protoEnum)
                return entry.implName;
        return "";
    }

    std::string Pool::getAlgoTypeForImplName(const std::string &implName) {
        if (auto entry = entryWithName(implName))
            return entry.value().algoName;
        return ""; //no matching algo type found
    }

    ProtoEnum Pool::getProtocolForImplName(const std::string &implName) {
        if (auto entry = entryWithName(implName))
            return entry.value().protoEnum;
        return kProtoCount;
    }

    auto Pool::entryWithName(const std::string &implName) -> optional_ref<Entry> {
        for (auto &entry : getEntries())
            if (entry.implName == implName)
                return type_safe::opt_ref(entry);
        return nullopt;
    }

}