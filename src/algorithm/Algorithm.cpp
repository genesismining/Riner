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
    static const Algorithm::Registry<AlgoCuckatoo31Cl> registryAlgoCuckatoo31Cl {"AlgoCuckatoo31Cl", HasPowTypeCuckatoo<31>::getPowType()};
    static const Algorithm::Registry<AlgoDummy>        regustryAlgoDummy        {"AlgoDummy"   , HasPowTypeEthash::getPowType()};
    static const Algorithm::Registry<AlgoEthashCL>     registryAlgoEthashCL     {"AlgoEthashCL", HasPowTypeEthash::getPowType()};


    unique_ptr<Algorithm> Algorithm::makeAlgo(AlgoConstructionArgs args, const std::string &implName) {
        if (auto entry = entryWithName(implName)) {
            return entry.value().makeFunc(std::move(args));
        }
        return nullptr;
    }

    std::string Algorithm::powTypeForAlgoImplName(const std::string &algoImplName) {
        if (auto entry = entryWithName(algoImplName)) {
            return entry.value().powType;
        }
        return ""; //no matching algo type found
    }

    auto Algorithm::entryWithName(const std::string &algoImplName) -> optional_ref<Entry> {
        for (auto &entry : getEntries()) {
            if (entry.algoImplName == algoImplName) {
                return type_safe::opt_ref(entry);
            }
        }
        return nullopt;
    }

}