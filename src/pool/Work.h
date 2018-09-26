
#pragma once

#include <src/common/WorkCommon.h>
#include <src/common/Pointers.h>

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