//
//

#pragma once

#include <src/pool/Pool.h>
#include <src/pool/Work.h>
#include <src/common/Pointers.h>
#include <src/util/Bytes.h>
#include <src/util/DifficultyTarget.h>

namespace riner {

    //your Work and WorkSolution structs both need to tell what their powType is
    //in their constructors.
    //
    //the naming scheme (e.g. for powType ethash) is as follows:
    //AlgoImpl: AlgoEthashCL, PoolImpl: PoolEthashStratum, Work: WorkEthash, WorkSolution: WorkSolutionEthash, PowType: "ethash"
    //
    //so for our dummy algorithm we choose "dummy" as the powtype, and the other
    //names accordingly. The below struct will help detect compatibility of Pool/Algo and Work/WorkSolution at runtime
    struct HasPowTypeDummy {
        static inline constexpr auto &getPowType() {
            return "dummy";
        }
    };

    //The work struct cointains everything the algorithm's implementation needs
    //to know to calculate a share for a given PowType efficiently, not more!
    //Pool communication related things (like job id) do NOT belong here (see "PoolJobDummy" for that)
    struct WorkDummy : public Work, public HasPowTypeDummy {

        //We need to tell the base class what our PowType is, so that riner can
        //tell us if we accidentially try to plug together a Pool of PowType A
        //with an Algo of PowType B
        WorkDummy() : Work(getPowType()) {
        }

        //The Work instances will most likely end up in a WorkQueue at some point
        //where they get duplicated and the "nonce" gets changed slightly.
        //for this duplication we need a copy-constructor.
        //(in our particular case it would be generated anyways. this is just for completeness' sake)
        WorkDummy(const WorkDummy &) = default;

        //now we add any data members that the AlgoImplDummy needs in order to
        //do its job (which is to find shares)
        Bytes<32> header;
        Bytes<32> target; //if our hash (interpreted as a number) is less than this target value, we found a solution!
        uint64_t nonce_begin = 0;
        uint64_t nonce_end = 0;
        double difficulty = 0;

        //we can also put helper functions here that will inevitably be needed in the
        //initialization of a Work struct, so that not every Dummy Pool implementation
        //has to reimplement them.
        void setDifficulty(double dfficulty) {
            difficulty = dfficulty;
            target = difficultyToTargetApprox(difficulty);
        }
    };

    //WorkSolution structs represent a found share of a given PowType, in this case of our "dummy" powtype.
    //Instances of WorkSolutionDummy are always created by having a WorkDummy
    //instance `w` and calling `w.makeWorkSolution<WorkSolutionDummy>();` on it.
    //
    struct WorkSolutionDummy : public WorkSolution, public HasPowTypeDummy {

        //here come all the data members that represent a found solution.
        //do not put pool communication related things like job-id here (see PoolJob for that).
        uint64_t nonce = 0;

        //for makeWorkSolution<...>(...) to work, we need to provide it with a
        //constructor like below, that takes a WorkDummy and hands it to the base class.
        WorkSolutionDummy(const WorkDummy &work)
        : WorkSolution(work) {
        }
    };

}
