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
            const std::string implName;
            const std::string algoName;
            ProtoEnum protoEnum;
            std::function<unique_ptr<Pool>(PoolConstructionArgs)> makeFunc;
        };

        static std::list<Entry> &getEntries() {
            static std::list<Entry> entries {};
            return entries;
        }

        static optional_ref<Entry> entryWithName(const std::string &implName);
        const Entry *info = nullptr;

    protected:
        static uint64_t createNewPoolUid();
        Pool() = default;

    public:
        virtual cstring_span getName() const = 0;
        virtual uint64_t getPoolUid() const = 0;
        virtual optional<unique_ptr<Work>> tryGetWork() = 0;
        virtual void submitWork(unique_ptr<WorkSolution> result) = 0;

        inline std::string getImplName() const {
            return info->implName;
        }

        inline std::string getAlgoName() const {
            return info->algoName;
        }

        //returns either nullopt or a valid unique_ptr (!= nullptr)
        template<class Work>
        optional<unique_ptr<Work>> tryGetWork() {
            auto maybeWork = tryGetWork();
            if (maybeWork) {
                auto &work = maybeWork.value();
                MI_EXPECTS(work != nullptr && work->algorithmName == Work::getName());
                return static_unique_ptr_cast<Work>(std::move(maybeWork.value()));
            }
            return nullopt;
        }

        template<class WorkSolution>
        void submitWork(unique_ptr<WorkSolution> result) {
            MI_EXPECTS(result != nullptr && result->algorithmName == WorkSolution::getName());

            submitWork(static_unique_ptr_cast<WorkSolution>(std::move(result)));
        }

        template<typename T>
        struct Registry {
            Registry(const std::string &name, const std::string &algoName, ProtoEnum protoEnum) noexcept {
                LOG(DEBUG) << "register Pool: " << name;

                getEntries().emplace_back(Entry{name, algoName, protoEnum});

                auto *entry = &getEntries().back();
                //create type erased creation function
                entry->makeFunc = [entry] (PoolConstructionArgs args) {
                    auto pool = std::make_unique<T>(std::move(args));
                    pool->info = entry;
                    return pool;
                };
            }

            Registry() = delete;
            DELETE_COPY_AND_MOVE(Registry);
        };

        //returns nullptr if no matching pool was found
        static unique_ptr<Pool> makePool(PoolConstructionArgs args, const std::string &poolImplName);
        static unique_ptr<Pool> makePool(PoolConstructionArgs args, const std::string &algoName, ProtoEnum);

        //returns implName
        static std::string getImplNameForAlgoTypeAndProtocol(const std::string &algoName, ProtoEnum);

        //returns empty string if implName doesn't match any pool impl
        static std::string getAlgoTypeForImplName(const std::string &implName);

        //returns kProtoCount if implName doesn't match any pool impl
        static ProtoEnum getProtocolForImplName(const std::string &implName);

        virtual ~Pool() = default;
        DELETE_COPY_AND_MOVE(Pool);
    };

}
