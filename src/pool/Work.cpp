
#include "Work.h"

namespace miner {

    WorkResultBase::WorkResultBase(std::weak_ptr<WorkProtocolData> protocolData)
    : protocolData(std::move(protocolData)) {
    }

}