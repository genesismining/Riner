#pragma once

#include <src/common/WorkCommon.h>
#include <src/common/Pointers.h>
#include <src/common/Optional.h>
#include <src/common/Assert.h>
#include <src/pool/Work.h>
#include <src/common/StringSpan.h>
#include <string>
#include <atomic>

namespace miner {
    struct PoolConstructionArgs {
        //don't confuse this with Config::Pool. PoolConstructionArgs may be used
        //to pass refs to other subsystems in the future (e.g. io_service)
        std::string host;
        std::string port;
        std::string username;
        std::string password;
    };

    class StillAliveTrackable {//WorkProviders extend this class to offer timestamp information of last incoming message to a PoolSwitcher
    public:
        using clock = std::chrono::system_clock;

        StillAliveTrackable() = default;
        virtual ~StillAliveTrackable() = default;

        clock::time_point getLastKnownAliveTime();
        void setLastKnownAliveTime(clock::time_point time);
        void onStillAlive(); //call this if you just received an incoming message from the network, it will update the timestamp

    private:
        std::atomic<clock::time_point> lastKnownAliveTime = {clock::time_point::min()};
    };

    class WorkProvider : public StillAliveTrackable {
    protected:
        static uint64_t createNewPoolUid();

    public:
        virtual cstring_span getName() const = 0;
        virtual uint64_t getPoolUid() const = 0;
        virtual optional<unique_ptr<WorkBase>> tryGetWork() = 0;
        virtual void submitWork(unique_ptr<WorkResultBase> result) = 0;

        //returns either nullopt or a valid unique_ptr (!= nullptr)
        template<AlgoEnum A>
        optional<unique_ptr<Work<A>>> tryGetWork() {
            auto maybeWork = tryGetWork();
            if (maybeWork) {
                auto &work = maybeWork.value();
                MI_EXPECTS(work != nullptr && work->getAlgoEnum() == A);
                return static_unique_ptr_cast<Work<A>>(std::move(maybeWork.value()));
            }
            return nullopt;
        }

        template<AlgoEnum A>
        void submitWork(unique_ptr<WorkResult<A>> result) {
            MI_EXPECTS(result != nullptr && result->getAlgoEnum() == A);

            submitWork(static_unique_ptr_cast<WorkResultBase>(std::move(result)));
        }

        virtual ~WorkProvider() = default;
    };

}
