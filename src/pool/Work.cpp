
#include "Work.h"

namespace miner {

    WorkSolution::WorkSolution(std::weak_ptr<WorkProtocolData> protocolData, const std::string &algorithmName)
    : protocolData(std::move(protocolData)), algorithmName(algorithmName) {
    }

    std::weak_ptr<WorkProtocolData> WorkSolution::getProtocolData() const {
        return protocolData;
    }

    Work::Work(std::weak_ptr<WorkProtocolData> protocolData, const std::string &algorithmName)
            : protocolData(std::move(protocolData)), algorithmName(algorithmName) {
    }

}