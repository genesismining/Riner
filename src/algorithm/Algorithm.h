

#pragma once

#include <src/compute/ComputeModule.h>
#include <src/util/Copy.h>
#include <src/util/ConfigUtils.h>
#include <src/pool/Pool.h>

#include <vector>

namespace miner {

    struct AlgoConstructionArgs {
        ComputeModule &compute;
        std::vector<std::reference_wrapper<Device>> assignedDevices;
        Pool &workProvider;
    };

    class Algorithm {
    public:
        virtual ~Algorithm() = default;
        DELETE_COPY_AND_MOVE(Algorithm);

        template<typename T>
        struct Registry {
            Registry(const std::string &name, AlgoEnum algoEnum) noexcept {

                //create type erased creation function
                decltype(Entry::makeFunc) makeFunc = [] (AlgoConstructionArgs args) {
                    return std::make_unique<T>(std::move(args));
                };

                getEntries().emplace_back(Entry{name, algoEnum, makeFunc});
            }

            Registry() = delete;
            Registry(const Registry &) = delete;
            Registry &operator=(const Registry &) = delete;
        };

        //returns nullptr if algoImplName doesn't match any registered algorithm
        static unique_ptr<Algorithm> makeAlgo(AlgoConstructionArgs args, const std::string &algoImplName);

        //returns kAlgoTypeCount if implName doesn't match any algo
        static AlgoEnum getAlgoTypeForImplName(const std::string &implName);

    protected:
        Algorithm() = default;

    private:
        struct Entry {
            std::string implName;
            AlgoEnum algoEnum;
            std::function<unique_ptr<Algorithm>(AlgoConstructionArgs)> makeFunc;
        };

        static std::list<Entry> &getEntries() {
            static std::list<Entry> entries {};
            return entries;
        }

        static optional_ref<Entry> entryWithName(const std::string &implName);

    };

}