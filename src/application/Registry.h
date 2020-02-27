

#pragma once

#include <functional>
#include <src/common/Pointers.h>
#include <src/algorithm/Algorithm.h>
#include <src/gpu_api/GpuApi.h>
#include <src/pool/Pool.h>
#include <map>
#include <vector>
#include <string>

namespace riner {

    struct AlgoConstructionArgs;
    struct PoolConstructionArgs;

    class Registry {
    public:
        Registry() {
            registerAllAlgoImpls();
            registerAllPoolImpls();
            registerAllGpuApis();
        }

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
        unique_ptr<Algorithm> makeAlgo(const char *name, AlgoConstructionArgs args) const;

        //may return nullptr if name wasn't found
        //shared_ptr for the owner so weak_ptrs of this object can be distributed
        shared_ptr<Pool> makePool(const char *name, PoolConstructionArgs args) const;

        //will return nullptr if the GpuApi could not be initialized or the name wasn't found
        unique_ptr<GpuApi> tryMakeGpuApi(const char *name, const GpuApiConstructionArgs &args) const;

        struct Listing { //return values of listAlgoImpls() and listPoolImpls()
            const char *name = "";
            const char *powType = "";
            const char *protocolType = ""; //only for PoolImpls
            const char *protocolTypeAlias = ""; //only for PoolImpls
        };

        std::vector<Listing> listAlgoImpls() const;
        std::vector<Listing> listPoolImpls() const;
        std::vector<const char *> listGpuApis() const;

        const char *powTypeOfAlgoImpl(const char *algoImplName) const; //returns empty string "" if not found
        const char *powTypeOfPoolImpl(const char *algoImplName) const; //returns empty string "" if not found
        const char *poolImplForProtocolAndPowType(const char *protocolType, const char *powType) const; // "" if not found


    private:
        void registerAllAlgoImpls();
        void registerAllPoolImpls();
        void registerAllGpuApis();

        struct EntryAlgo {
            const char *powType;
            std::function<unique_ptr<Algorithm>(AlgoConstructionArgs &&)> makeFunc;
        };

        struct EntryPool {
            const char *powType = "";
            const char *protocolType = "";
            const char *protocolTypeAlias = "";
            std::function<shared_ptr<Pool>(PoolConstructionArgs &&)> makeFunc;
        };

        struct EntryGpuApi {
            std::function<std::unique_ptr<GpuApi>(const GpuApiConstructionArgs &)> tryMakeFunc; //can fail => nullptr
        };

        std::map<const char *, EntryAlgo> _algoWithName;
        std::map<const char *, EntryPool> _poolWithName;
        std::map<const char *, EntryGpuApi> _gpuApiWithName;

        template<class AlgoT>
        void addAlgoImpl(const char *algoImplName, const char *powType) {
            static_assert(std::is_base_of<Algorithm, AlgoT>::value, "AlgoT must derive from Algorithm");
            RNR_EXPECTS(_algoWithName.count(algoImplName) == 0); //don't add two algos with the same name!

            _algoWithName[algoImplName] = {
                    powType,
                    [] (AlgoConstructionArgs &&args) -> unique_ptr<Algorithm> {
                        return make_unique<AlgoT>(std::move(args));
                    }
            };
        }

        template<class PoolT>
        void addPoolImpl(const char *poolImplName, const char *powType, const char *protocolType, const char *protocolTypeAlias = "") {
            static_assert(std::is_base_of<Pool, PoolT>::value, "PoolT must derive from Pool");
            RNR_EXPECTS(_poolWithName.count(poolImplName) == 0); //don't add two pools with the same name!

            _poolWithName[poolImplName] = {
                    powType,
                    protocolType,
                    protocolTypeAlias,
                    [=] (PoolConstructionArgs &&args) -> shared_ptr<Pool> {
                        auto pool = make_shared<PoolT>(std::move(args));
                        Pool::postInit(pool, poolImplName, powType); //set the _this ptr in pool
                        return pool;
                    }
            };
        }

        template<class GpuApiT>
        void addGpuApi(const char *gpuApiName) {
            static_assert(std::is_base_of<GpuApi, GpuApiT>::value, "GpuApiT must derive from GpuApi");
            RNR_EXPECTS(_gpuApiWithName.count(gpuApiName) == 0); //don't add two gpuApis with the same name!

            _gpuApiWithName[gpuApiName] = {
                    [] (const GpuApiConstructionArgs &args) -> unique_ptr<GpuApi> {
                        return GpuApiT::tryMake(args);
                    }
            };
        }
    };

}
