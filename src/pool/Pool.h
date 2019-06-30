#pragma once

#include <src/common/WorkCommon.h>
#include <src/common/Pointers.h>
#include <src/common/Optional.h>
#include <src/common/Assert.h>
#include <src/util/Copy.h>
#include <src/pool/Work.h>
#include <src/common/StringSpan.h>
#include <string>
#include <list>
#include <atomic>
#include <chrono>

namespace miner {
    class PoolRecords;
    
    struct PoolConstructionArgs {
        //don't confuse this with Config::Pool. PoolConstructionArgs may be used
        //to pass refs to other subsystems in the future (e.g. io_service)
        std::string host;
        uint16_t port;
        std::string username;
        std::string password;
        PoolRecords &poolRecords;
    };

    class StillAliveTrackable {//pools extend this class to offer timestamp information of last incoming message to a PoolSwitcher
    public:
        using clock = std::chrono::system_clock;

        StillAliveTrackable() = default;
        virtual ~StillAliveTrackable() = default;

        DELETE_COPY(StillAliveTrackable);

        clock::time_point getLastKnownAliveTime();
        void setLastKnownAliveTime(clock::time_point time);
        void onStillAlive(); //call this if you just received an incoming message from the network, it will update the timestamp

    private:
        std::atomic<clock::time_point> lastKnownAliveTime = {clock::time_point::min()};
    };

    class Pool : public StillAliveTrackable {

        struct Entry {
            std::string implName;
            AlgoEnum algoEnum;
            ProtoEnum protoEnum;
            std::function<unique_ptr<Pool>(PoolConstructionArgs)> makeFunc;
        };

        static std::list<Entry> &getEntries() {
            static std::list<Entry> entries {};
            return entries;
        }

        static optional_ref<Entry> entryWithName(const std::string &implName);

    protected:
        static uint64_t createNewPoolUid();
        Pool() = default;

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

        template<typename T>
        struct Registry {
            Registry(const std::string &name, AlgoEnum algoEnum, ProtoEnum protoEnum) noexcept {
                //create type erased creation function
                auto makeFunc = [] (PoolConstructionArgs args) {
                    return std::make_unique<T>(std::move(args));
                };

                getEntries().emplace_back(Entry{name, algoEnum, protoEnum, makeFunc});
            }
        };

        //returns nullptr if no matching pool was found
        static unique_ptr<Pool> makePool(PoolConstructionArgs args, const std::string &poolImplName);
        static unique_ptr<Pool> makePool(PoolConstructionArgs args, AlgoEnum, ProtoEnum);

        //returns implName
        static std::string getImplNameForAlgoTypeAndProtocol(AlgoEnum, ProtoEnum);

        //returns kAlgoTypeCount if implName doesn't match any pool impl
        static AlgoEnum getAlgoTypeForImplName(const std::string &implName);

        //returns kProtoCount if implName doesn't match any pool impl
        static ProtoEnum getProtocolForImplName(const std::string &implName);

        virtual ~Pool() = default;
        Pool(const Pool &) = delete;
        Pool &operator=(const Pool &) = delete;
    };

}
