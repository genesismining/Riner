
#pragma once

#include <src/pool/Pool.h>
#include <src/common/Pointers.h>
#include <src/common/WorkCommon.h>
#include <string>
#include <vector>
#include <functional>
#include <src/common/Optional.h>

namespace miner {

    class PoolFactory {

        struct Entry {
            std::string implName;
            AlgoEnum algoEnum;
            ProtoEnum protoEnum;
            std::function<unique_ptr<WorkProvider>(PoolConstructionArgs)> makeFunc;
        };

        std::vector<Entry> entries;

        optional_ref<Entry> entryWithName(const std::string &implName);

    public:
        PoolFactory();

        //returns nullptr if no matching pool was found
        unique_ptr<WorkProvider> makePool(PoolConstructionArgs args, const std::string &algoImplName);
        unique_ptr<WorkProvider> makePool(PoolConstructionArgs args, AlgoEnum, ProtoEnum);

        //returns kAlgoTypeCount if implName doesn't match any algo
        AlgoEnum getAlgoTypeForImplName(const std::string &implName);

        //returns kProtoCount if implName doesn't match any algo
        ProtoEnum getProtocolForImplName(const std::string &implName);

        template<class T>
        void registerType(const std::string &name, AlgoEnum algoEnum, ProtoEnum protoEnum) {

            //create type erased creation function
            auto makeFunc = [] (PoolConstructionArgs args) {
                return std::make_unique<T>(std::move(args));
            };

            entries.emplace_back(Entry{name, algoEnum, protoEnum, makeFunc});
        }

    };

}