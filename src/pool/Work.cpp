
#include "Work.h"

namespace miner {

    WorkSolution::WorkSolution(std::weak_ptr<WorkProtocolData> protocolData, const std::string &powType)
    : protocolData(std::move(protocolData)), powType(powType) {
    }

    std::weak_ptr<WorkProtocolData> WorkSolution::getProtocolData() const {
        return protocolData;
    }

    Work::Work(std::weak_ptr<WorkProtocolData> protocolData, const std::string &powType)
            : protocolData(std::move(protocolData)), powType(powType) {
    }

}