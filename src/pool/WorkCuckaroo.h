
#pragma once

#include <src/algorithm/grin/siphash.h>
#include <src/common/Pointers.h>
#include <src/pool/Pool.h>
#include <src/pool/Work.h>
#include <src/util/Bytes.h>

namespace miner {

    template<>
    class Work<kCuckaroo31> : public WorkBase {
    public:
        Work(std::weak_ptr<WorkProtocolData> data) : WorkBase(std::move(data)) {}

        SiphashKeys keys;

        AlgoEnum getAlgoEnum() const override {
            return kCuckaroo31;
        }
    };

    template<>
    class WorkResult<kCuckaroo31> : public WorkResultBase {
    public:
        WorkResult(std::weak_ptr<WorkProtocolData> data) : WorkResultBase(std::move(data)) {}

        AlgoEnum getAlgoEnum() const override {
            return kCuckaroo31;
        }
    };

}







