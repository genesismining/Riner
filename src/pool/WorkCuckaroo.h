#pragma once

#include <src/common/Pointers.h>
#include <src/pool/Pool.h>
#include <src/pool/Work.h>
#include <src/util/Bytes.h>

#include <vector>

namespace miner {

template<>
class Work<kCuckatoo31> : public WorkBase {
public:
    Work(std::weak_ptr<WorkProtocolData> data) :
            WorkBase(std::move(data)) {
    }

    int64_t difficulty = 1;
    int64_t jobId = -1;
    int64_t height = 0;
    std::vector<uint8_t> prePow;

    uint64_t nonce = 0;

    AlgoEnum getAlgoEnum() const override {
        return kCuckatoo31;
    }
};

template<>
class WorkResult<kCuckatoo31> : public WorkResultBase {
public:
    WorkResult(std::weak_ptr<WorkProtocolData> data) :
            WorkResultBase(std::move(data)) {
    }

    AlgoEnum getAlgoEnum() const override {
        return kCuckatoo31;
    }

    int64_t height = 0;
    uint64_t nonce = 0;
    std::vector<uint32_t> pow;
};

} // namespace miner

