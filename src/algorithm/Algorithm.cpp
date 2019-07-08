//
//

#include "Algorithm.h"
#include "grin/AlgoCuckatoo31Cl.h"
#include "dummy/AlgoDummy.h"
#include "ethash/AlgoEthashCL.h"
#include <src/compute/DeviceId.h>
#include <src/pool/WorkEthash.h>
#include <src/pool/WorkCuckoo.h>

namespace miner {

    /**
     * Register all Algorithm implementations here.
     * This is necessary so that the compiler includes the necessary code into a library
     * because only Algorithm is referenced by another compilation unit.
     */
    static const Algorithm::Registry<AlgoCuckatoo31Cl> cuckatoo31Registry {"cuckatoo31", AlgorithmCuckatoo31::getName()};
    static Algorithm::Registry<AlgoDummy> registry {"dummy", AlgorithmEthash::getName()};
    static const Algorithm::Registry<AlgoEthashCL> ethashRegistry {"ethash", AlgorithmEthash::getName()};


    unique_ptr<Algorithm> Algorithm::makeAlgo(AlgoConstructionArgs args, const std::string &implName) {
        if (auto entry = entryWithName(implName)) {
            return entry.value().makeFunc(std::move(args));
        }
        return nullptr;
    }

    std::string Algorithm::implNameToAlgoName(const std::string &implName) {
        if (auto entry = entryWithName(implName)) {
            return entry.value().algorithmName;
        }
        return ""; //no matching algo type found
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