
#include "Work.h"

namespace miner {

    WorkResultBase::WorkResultBase(std::weak_ptr<WorkProtocolData> protocolData)
    : protocolData(std::move(protocolData)) {
    }

    std::weak_ptr<WorkProtocolData> WorkResultBase::getProtocolData() const {
        return protocolData;
    }

    WorkBase::WorkBase(std::weak_ptr<WorkProtocolData> protocolData)
            : protocolData(std::move(protocolData)) {
    }

}