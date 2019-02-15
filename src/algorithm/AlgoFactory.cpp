//
//

#include "AlgoFactory.h"
#include <src/compute/DeviceId.h>
#include <src/application/Types.h>

namespace miner {

    AlgoFactory::AlgoFactory() {
        registerAlgoTypes(*this);
    }

    unique_ptr<AlgoBase> AlgoFactory::makeAlgo(AlgoConstructionArgs args, const std::string &implName) {
        if (auto entry = entryWithName(implName)) {
            return entry.value().makeFunc(std::move(args));
        }
        return nullptr;
    }

    AlgoEnum AlgoFactory::getAlgoTypeForImplName(const std::string &implName) {
        if (auto entry = entryWithName(implName)) {
            return entry.value().algoEnum;
        }
        return kAlgoTypeCount; //no matching algo type found
    }

    optional_ref<AlgoFactory::Entry> AlgoFactory::entryWithName(const std::string &implName) {
        for (auto &entry : entries) {
            if (entry.implName == implName) {
                return type_safe::opt_ref(entry);
            }
        }
        return nullopt;
    }

}
