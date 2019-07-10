
#pragma once

#include <src/common/WorkCommon.h>
#include <src/common/Pointers.h>
#include <src/common/Optional.h>

namespace miner {

    class WorkProtocolData {
        uint64_t poolUid = 0; //used to determine whether a WorkResult is
        // returned to the same pool that created the work to begin with.
    public:
        WorkProtocolData(uint64_t poolUid) : poolUid(poolUid) {
        };

        uint64_t getPoolUid() {
            return poolUid;
        }
    };

    class WorkSolution {
        std::weak_ptr<WorkProtocolData> protocolData;

    protected:
        WorkSolution(std::weak_ptr<WorkProtocolData>, const std::string &);

    public:
        const std::string algorithmName;

        std::weak_ptr<WorkProtocolData> getProtocolData() const;

        template<class T>
        std::shared_ptr<T> tryGetProtocolDataAs() const {
            if (auto pdata = protocolData.lock()) {
                return std::static_pointer_cast<T>(pdata);
            }
            return nullptr;
        }

        virtual ~WorkSolution() = default;
    };

    class Work {
        std::weak_ptr<WorkProtocolData> protocolData;

    protected:
        Work(std::weak_ptr<WorkProtocolData>, const std::string &);

    public:
        const std::string algorithmName;
        virtual ~Work() = default;

        bool expired() const {
            //checks whether associated shared_ptr is still alive
            return protocolData.expired();
        }

        template<class T>
        unique_ptr<T> makeWorkSolution() const {
            return std::make_unique<T>(protocolData);
        }
    };
}
