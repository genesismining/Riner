//
//

#include "PoolFactory.h"
#include <src/application/Types.h>

namespace miner {

    PoolFactory::PoolFactory() {
        registerPoolTypes(*this);
    }

    unique_ptr<WorkProvider> PoolFactory::makePool(PoolConstructionArgs args, const std::string &implName) {
        if (auto entry = entryWithName(implName))
            return entry.value().makeFunc(std::move(args));
        return nullptr;
    }

    AlgoEnum PoolFactory::getAlgoTypeForImplName(const std::string &implName) {
        if (auto entry = entryWithName(implName))
            return entry.value().algoEnum;
        return kAlgoTypeCount; //no matching algo type found
    }

    ProtoEnum PoolFactory::getProtocolForImplName(const std::string &implName) {
        if (auto entry = entryWithName(implName))
            return entry.value().protoEnum;
        return kProtoCount;
    }

    auto PoolFactory::entryWithName(const std::string &implName) -> optional_ref<Entry> {
        for (auto &entry : entries)
            if (entry.implName == implName)
                return type_safe::opt_ref(entry);
        return nullopt;
    }

    unique_ptr<WorkProvider> PoolFactory::makePool(PoolConstructionArgs args, AlgoEnum algoEnum, ProtoEnum protoEnum) {
        for (auto &entry : entries)
            if (entry.algoEnum == algoEnum && entry.protoEnum == protoEnum)
                return entry.makeFunc(std::move(args));
        return nullptr;
    }

}