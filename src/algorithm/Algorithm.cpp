//
//

#include "Algorithm.h"
#include <src/compute/DeviceId.h>

namespace miner {

    unique_ptr<Algorithm> Algorithm::makeAlgo(AlgoConstructionArgs args, const std::string &implName) {
        if (auto entry = entryWithName(implName)) {
            return entry.value().makeFunc(std::move(args));
        }
        return nullptr;
    }

    AlgoEnum Algorithm::getAlgoTypeForImplName(const std::string &implName) {
        if (auto entry = entryWithName(implName)) {
            return entry.value().algoEnum;
        }
        return kAlgoTypeCount; //no matching algo type found
    }

    auto Algorithm::entryWithName(const std::string &implName) -> optional_ref<Entry> {
        for (auto &entry : getEntries()) {
            if (entry.implName == implName) {
                return type_safe::opt_ref(entry);
            }
        }
        return nullopt;
    }

}