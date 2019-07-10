#pragma once

#include <src/common/Pointers.h>
#include <src/pool/Pool.h>
#include <src/pool/Work.h>
#include <src/util/Bytes.h>

#include <vector>

namespace miner {

struct AlgorithmCuckatoo31 {
    static inline constexpr auto &getName() {
        return "cuckatoo31";
    }
};

template<class Algorithm>
class WorkCuckoo : public Work, public Algorithm {
public:
    WorkCuckoo(std::shared_ptr<WorkProtocolData> data) :
            Work(make_weak(data), Algorithm::getName()) {
    }

    int64_t difficulty = 1;
    int64_t height = 0;
    std::vector<uint8_t> prePow;

    uint64_t nonce = 0;
};

template<class Algorithm>
class WorkSolutionCuckoo : public WorkSolution, public Algorithm {
public:
    WorkSolutionCuckoo(std::weak_ptr<WorkProtocolData> data) :
            WorkSolution(std::move(data), Algorithm::getName()) {
    }

    int64_t height = 0;
    uint64_t nonce = 0;
    std::vector<uint32_t> pow;
};

typedef WorkCuckoo<AlgorithmCuckatoo31> WorkCuckatoo31;
typedef WorkSolutionCuckoo<AlgorithmCuckatoo31> POWCuckatoo31;

} // namespace miner

