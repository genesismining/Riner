#pragma once

#include <src/common/Pointers.h>
#include <src/pool/Pool.h>
#include <src/pool/Work.h>
#include <src/util/Bytes.h>

#include <vector>

namespace miner {

struct HasPowTypeCuckatoo31 {
    static inline constexpr auto &getPowType() {
        return "cuckatoo31";
    }
};

template<class PowTypeT>
class WorkCuckoo : public Work, public PowTypeT {
public:
    WorkCuckoo() :
            Work(PowTypeT::getPowType()) {
    }

    int64_t difficulty = 1;
    std::vector<uint8_t> prePow;

    uint64_t nonce = 0;
};

template<class PowTypeT>
class WorkSolutionCuckoo : public WorkSolution, public PowTypeT {
public:
    using work_type = WorkCuckoo<PowTypeT>;

    WorkSolutionCuckoo(const WorkCuckoo<PowTypeT> &work) :
            WorkSolution(static_cast<const Work&>(work)) {
    }

    uint64_t nonce = 0;
    std::vector<uint32_t> pow;
};

using WorkCuckatoo31 = WorkCuckoo<HasPowTypeCuckatoo31>;
using WorkSolutionCuckatoo31 = WorkSolutionCuckoo<HasPowTypeCuckatoo31>;

} // namespace miner

