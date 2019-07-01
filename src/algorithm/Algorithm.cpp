//
//

#include "Algorithm.h"
#include "ethash/AlgoEthashCL.h"
#include "grin/AlgoCuckatoo31Cl.h"
#include <src/compute/DeviceId.h>

namespace miner {

    /**
     * Register all Algorithm implementations here.
     * This is necessary so that the compiler includes the necessary code into a library
     * because only Algorithm is referenced by another compilation unit.
     */
    static const Algorithm::Registry<AlgoCuckatoo31Cl> cuckatoo31Registry {"cuckatoo31", kCuckatoo31};
    static const Algorithm::Registry<AlgoEthashCL> ethashRegistry {"ethash", kEthash};


    unique_ptr<Algorithm> Algorithm::makeAlgo(AlgoConstructionArgs args, const std::string &implName) {
        if (auto entry = entryWithName(implName)) {
            return entry.value().makeFunc(std::move(args));
        }
        return nullptr;
    }

    AlgoEnum Algorithm::stringToAlgoEnum(const std::string &implName) {
        if (auto entry = entryWithName(implName)) {
            return entry.value().algoEnum;
        }
        return kAlgoTypeCount; //no matching algo type found
    }

    std::string Algorithm::algoEnumToString(AlgoEnum algoEnum) {
        for (auto &entry : getEntries()) {
            if (entry.algoEnum == algoEnum) {
                return entry.implName;
            }
        }
        return "invalid algo type";
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