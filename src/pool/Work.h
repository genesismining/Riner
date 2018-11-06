
#pragma once

#include <src/common/WorkCommon.h>
#include <src/common/Pointers.h>
#include <src/common/Optional.h>

namespace miner {
    class WorkBase;

    class WorkProtocolData {
        uint64_t poolUid = 0; //used to determine whether a WorkResult is
        // returned to the same pool that created the work to begin with.

    };

    class WorkResultBase {
        std::weak_ptr<WorkProtocolData> protocolData;
    public:
        WorkResultBase(std::weak_ptr<WorkProtocolData>);

        std::weak_ptr<WorkProtocolData> getProtocolData() const;

        template<class T>
        optional<std::shared_ptr<T>> tryGetProtocolDataAs() const {
            optional<std::shared_ptr<T>> opt;
            if (auto pdata = protocolData.lock()) {
                opt = std::move(std::static_pointer_cast<T>(pdata));
            }
            return opt;
        }

        virtual AlgoEnum getAlgoEnum() const = 0;
        virtual ~WorkResultBase() = default;
    };

    template<AlgoEnum A>
    class WorkResult;

    class WorkBase {
        std::weak_ptr<WorkProtocolData> protocolData;
    public:
        virtual AlgoEnum getAlgoEnum() const = 0;
        virtual ~WorkBase() = default;

        WorkBase(std::weak_ptr<WorkProtocolData>);

        bool expired() const {
            //checks whether associated shared_ptr is still alive
            return protocolData.expired();
        }

        template<AlgoEnum A>
        unique_ptr<WorkResult<A>> makeWorkResult() const {
            return std::make_unique<WorkResult<A>>(protocolData);
        }
    };

    template<AlgoEnum A>
    class Work;

    template<> class Work<kEthash>;
    template<> class WorkResult<kEthash>;

    //template<> class Work<kEquihash>;
    //template<> class WorkResult<kEquihash>;

    //template<> class Work<kCryptoNight>;
    //template<> class WorkResult<kCryptoNight>;
}