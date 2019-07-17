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
    WorkCuckoo(std::shared_ptr<WorkProtocolData> data) :
            Work(make_weak(data), PowTypeT::getPowType()) {
    }

    int64_t difficulty = 1;
    int64_t height = 0;
    std::vector<uint8_t> prePow;

    uint64_t nonce = 0;
};

template<class PowTypeT>
class WorkSolutionCuckoo : public WorkSolution, public PowTypeT {
public:
    WorkSolutionCuckoo(std::weak_ptr<WorkProtocolData> data) :
            WorkSolution(std::move(data), PowTypeT::getPowType()) {
    }

    int64_t height = 0;
    uint64_t nonce = 0;
    std::vector<uint32_t> pow;
};

using WorkCuckatoo31 = WorkCuckoo<PowTypeCuckatoo31>;
using WorkSolutionCuckatoo31 = WorkSolutionCuckoo<PowTypeCuckatoo31>;

} // namespace miner

