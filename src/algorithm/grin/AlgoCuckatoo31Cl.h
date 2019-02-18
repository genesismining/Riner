#pragma once

#include <src/algorithm/Algorithm.h>
#include <lib/cl2hpp/include/cl2.hpp>
#include <src/algorithm/grin/Cuckatoo.h>
#include <atomic>
#include <vector>
#include <thread>

namespace miner {

typedef Work<kCuckatoo31> CuckooHeader;
typedef WorkResult<kCuckatoo31> CuckooPow;

class AlgoCuckatoo31Cl: public AlgoBase {

public:
    explicit AlgoCuckatoo31Cl(AlgoConstructionArgs args);
    ~AlgoCuckatoo31Cl() override;

    static SiphashKeys calculateKeys(const CuckooHeader& header);

private:
    void run(cl::Context& context, CuckatooSolver& solver);

    std::atomic<bool> terminate_;

    AlgoConstructionArgs args_;
    std::vector<std::thread> workers_;

};

} /* namespace miner */

