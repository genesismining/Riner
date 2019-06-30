
#include "Pool.h"

namespace miner {

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

    unique_ptr<Pool> Pool::makePool(PoolConstructionArgs args, AlgoEnum algoEnum, ProtoEnum protoEnum) {
        for (auto &entry : getEntries())
            if (entry.algoEnum == algoEnum && entry.protoEnum == protoEnum)
                return entry.makeFunc(std::move(args));
        return nullptr;
    }

    std::string Pool::getImplNameForAlgoTypeAndProtocol(AlgoEnum algoEnum, ProtoEnum protoEnum) {
        for (auto &entry : getEntries())
            if (entry.algoEnum == algoEnum && entry.protoEnum == protoEnum)
                return entry.implName;
        return "";
    }

    AlgoEnum Pool::getAlgoTypeForImplName(const std::string &implName) {
        if (auto entry = entryWithName(implName))
            return entry.value().algoEnum;
        return kAlgoTypeCount; //no matching algo type found
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