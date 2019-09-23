#pragma once

#include <src/algorithm/Algorithm.h>
#include <src/algorithm/grin/Cuckatoo.h>
#include <src/util/TaskExecutorPool.h>
#include <atomic>
#include <vector>
#include <thread>

namespace miner {

class AlgoCuckatoo31Cl: public Algorithm {

public:
    explicit AlgoCuckatoo31Cl(AlgoConstructionArgs args);
    ~AlgoCuckatoo31Cl() override;

    static SiphashKeys calculateKeys(const WorkCuckatoo31& header);

private:
    void run(cl::Context& context, CuckatooSolver& solver);

    std::atomic<bool> terminate_;

    AlgoConstructionArgs args_;
    TaskExecutorPool tasks {std::thread::hardware_concurrency()};
    std::vector<std::thread> workers_;
};

} /* namespace miner */

