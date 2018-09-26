#pragma once

#include <src/common/WorkCommon.h>
#include <src/common/Pointers.h>
#include <src/common/Assert.h>
#include <src/pool/Work.h>

namespace miner {
    class WorkResultBase;
    class WorkBase;
    template<AlgoEnum A> class WorkResult;
    template<AlgoEnum A> class Work;

    class WorkProvider {
    protected:
        virtual unique_ptr<WorkBase> getWork() = 0;
        virtual void submitWork(unique_ptr<WorkResultBase> result) = 0;

    public:
        template<AlgoEnum A>
        unique_ptr<Work<A>> getWork() {
            auto work = getWork();
            MI_EXPECTS(work->getAlgoEnum() == A);
            return static_unique_ptr_cast<Work<A>>(std::move(work));
        }

        template<AlgoEnum A>
        void submitWork(unique_ptr<WorkResult<A>> result) {
            MI_EXPECTS(result->getAlgoEnum() == A);

            submitWork(static_unique_ptr_cast<WorkResultBase>(std::move(result)));
        }

        virtual ~WorkProvider() = default;
    };

}