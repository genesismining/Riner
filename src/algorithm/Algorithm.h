

#pragma once

#include <src/compute/ComputeModule.h>
#include <src/util/Copy.h>
#include <src/util/ConfigUtils.h>
#include <src/pool/Pool.h>

#include <vector>

namespace miner {

    /**
     * @brief Args passed into every Algorithm subclass ctor
     * If you want to add args to every algorithm ctor, add them to this struct instead
     */
    struct AlgoConstructionArgs {
        ComputeModule &compute;
        std::vector<std::reference_wrapper<Device>> assignedDevices;
        Pool &workProvider;
    };

    /**
     * @brief common base class for all AlgoImpls
     * contains facilities for registering AlgoImpls in a global list, so that the appropriate AlgoImpl class can be
     * selected and instantiated based on a name string
     * when subclassing Algorithm, do not forget to add your algorithm to the registry in 'Algorithm.cpp'
     */
    class Algorithm {
    public:
        virtual ~Algorithm() = default;

        DELETE_COPY_AND_MOVE(Algorithm);

        /**
         * used in Algorithm.cpp to register AlgoImpls (= associate AlgoImpls with their name strings)
         * @tparam T AlgoImpl type
         */
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

        /**
         * @brief instantiates (runs) the AlgoImpl type that matches a name string
         * AlgoImpl classes are written in such a way that they start mining as soon as they are instantiated and
         * stop as they are destroyed, thus this call starts the mining process (provided the algorithm can obtain
         * work from the Pool referenced by args)
         *
         * @param args AlgoConstructionArgs that are forwarded into the selected AlgoImpl class' ctor
         * @param algoImplName string name that specifies the AlgoImpl class
         * @return type erased AlgoImpl or nullptr if algoImplName matches no registered AlgoImpl class
         */
        static unique_ptr<Algorithm> makeAlgo(AlgoConstructionArgs args, const std::string &algoImplName);

        //returns empty string if implName doesn't match any algo
        static std::string implNameToAlgoName(const std::string &implName);

        /**
         * get the POWtype that this algorithm implements (e.g. in order to find a matching PoolImpl that provides work for this POWtype)
         * @return
         */
        inline std::string getAlgoName() const {
            return info->algorithmName;
        }

        /**
         * @return name string that uniquely identifies the AlgoImpl class (subclass of Algorithm)
         */
        inline std::string getImplName() const {
            return info->implName;
        }

    protected:
        Algorithm() = default;

    private:
        /**
         * necessary information for instantiating AlgoImpls based on name string
         */
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