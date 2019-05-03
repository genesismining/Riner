
#pragma once

#include <src/algorithm/Algorithm.h>
#include <src/common/Pointers.h>
#include <src/common/WorkCommon.h>
#include <src/compute/DeviceId.h>
#include <string>
#include <list>
#include <functional>
#include <src/common/Optional.h>

namespace miner {

    class AlgoFactory {

        struct Entry {
            std::string implName;
            AlgoEnum algoEnum;
            std::function<unique_ptr<AlgoBase>(AlgoConstructionArgs)> makeFunc;
        };

        std::list<Entry> entries;

        optional_ref<Entry> entryWithName(const std::string &implName);

    public:
        AlgoFactory();

        //returns nullptr if algoImplName doesn't match any registered algorithm
        unique_ptr<AlgoBase> makeAlgo(AlgoConstructionArgs args, const std::string &algoImplName);

        //returns kAlgoTypeCount if implName doesn't match any algo
        AlgoEnum getAlgoTypeForImplName(const std::string &implName);

        template<class T>
        void registerType(const std::string &name, AlgoEnum algoEnum) {

            //create type erased creation function
            decltype(Entry::makeFunc) makeFunc = [] (AlgoConstructionArgs args) {
                return std::make_unique<T>(std::move(args));
            };

            entries.emplace_back(Entry{name, algoEnum, makeFunc});
        }

    };

}