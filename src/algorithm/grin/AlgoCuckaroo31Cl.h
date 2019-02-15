#pragma once

#include <src/algorithm/Algorithm.h>
#include <lib/cl2hpp/include/cl2.hpp>
#include <src/algorithm/grin/Cuckaroo.h>

#include <atomic>
#include <vector>
#include <thread>

namespace miner {

class AlgoCuckaroo31Cl: public AlgoBase {

public:
    explicit AlgoCuckaroo31Cl(AlgoConstructionArgs args);
    ~AlgoCuckaroo31Cl() override;

private:
    void run(cl::Context& context, CuckatooSolver& solver);

    std::atomic<bool> terminate_;

    AlgoConstructionArgs args_;
    std::vector<std::thread> workers_;

};

} /* namespace miner */

