

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
            Registry(const std::string &implName, const std::string &algoName) noexcept {
                LOG(DEBUG) << "register Algorithm: " << implName;

                getEntries().emplace_back(Entry{implName, algoName});

                auto *entry = &getEntries().back();
                //create type erased creation function
                entry->makeFunc = [entry] (AlgoConstructionArgs args) {
                    auto algorithm = std::make_unique<T>(std::move(args));
                    algorithm->info = entry;
                    return algorithm;
                };
            }

            Registry() = delete;
            DELETE_COPY_AND_MOVE(Registry);
        };

        //returns nullptr if algoImplName doesn't match any registered algorithm
        static unique_ptr<Algorithm> makeAlgo(AlgoConstructionArgs args, const std::string &algoImplName);

        //returns empty string if implName doesn't match any algo
        static std::string implNameToAlgoName(const std::string &implName);

        inline std::string getAlgoName() const {
            return info->algorithmName;
        }

        inline std::string getImplName() const {
            return info->implName;
        }

    protected:
        Algorithm() = default;

    private:
        struct Entry {
            const std::string implName;
            const std::string algorithmName;
            std::function<unique_ptr<Algorithm>(AlgoConstructionArgs)> makeFunc;
        };

        static std::list<Entry> &getEntries() {
            static std::list<Entry> entries {};
            return entries;
        }

        static optional_ref<Entry> entryWithName(const std::string &implName);
        const Entry *info = nullptr;

    };

}