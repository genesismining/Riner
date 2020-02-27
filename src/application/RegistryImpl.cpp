//
//

#include <src/application/Registry.h>
#include <src/common/Optional.h>
#include <strings.h>

namespace riner {

    //try to get element in map with key, not case sensitive, return nullptr if key doesn't match anything
    template <class Map>
    optional<const typename Map::mapped_type &> map_at_case_insensitive(Map &smap, const char *key) {
        for (auto &pair : smap)
            if (0 == strcasecmp(pair.first, key))
                return {pair.second};
        return nullopt;
    }

    unique_ptr<Algorithm> Registry::makeAlgo(const char *name, AlgoConstructionArgs args) const {
        if (_algoWithName.count(name))
            return _algoWithName.at(name).makeFunc(std::move(args)); //initFunc declared in Registry::addAlgoImpl<T>
        return nullptr;
    }

    //may return nullptr
    shared_ptr<Pool> Registry::makePool(const char *name, PoolConstructionArgs args) const {
        if (auto e = map_at_case_insensitive(_poolWithName, name))
            return e->makeFunc(std::move(args)); //initFunc declared in Registry::addAlgoImpl<T>
        return nullptr;
    }

    unique_ptr<GpuApi> Registry::tryMakeGpuApi(const char *name, const GpuApiConstructionArgs &args) const {
        if (auto e = map_at_case_insensitive(_gpuApiWithName, name))
            return e->tryMakeFunc(args); //initFunc declared in Registry::addAlgoImpl<T>
        return nullptr;
    }

    std::vector<Registry::Listing> Registry::listAlgoImpls() const {
        std::vector<Listing> ret;

        for (auto &pair : _algoWithName) {
            ret.emplace_back();
            auto &r = ret.back();
            r.name = pair.first;
            r.powType = pair.second.powType;
        }
        return ret;
    }

    std::vector<Registry::Listing> Registry::listPoolImpls() const {
        std::vector<Listing> ret;

        for (auto &pair : _poolWithName) {
            ret.emplace_back();
            auto &r = ret.back();
            r.name = pair.first;
            r.powType = pair.second.powType;
            r.protocolType = pair.second.protocolType;
            r.protocolTypeAlias = pair.second.protocolTypeAlias;
        }
        return ret;
    }

    std::vector<const char *> Registry::listGpuApis() const {
        std::vector<const char *> ret;

        for (auto &pair : _poolWithName) {
            ret.emplace_back(pair.first);
        }
        return ret;
    }

    const char *Registry::powTypeOfAlgoImpl(const char *algoImplName) const {
        if (auto e = map_at_case_insensitive(_algoWithName, algoImplName))
            return e->powType;
        return "";
    }

    const char *Registry::powTypeOfPoolImpl(const char *poolImplName) const {
        if (auto e = map_at_case_insensitive(_poolWithName, poolImplName))
            return e->powType;
        return "";
    }

    const char *Registry::poolImplForProtocolAndPowType(const char *protocolType, const char *powType) const {
        if (0 != strcmp(protocolType, "")) { //don't accidentally match "" with an unspecified protocolTypeAlias
            for (auto &pair : _poolWithName) {
                bool samePow = 0 == strcmp(pair.second.powType, powType);

                bool sameProto = 0 == strcasecmp(pair.second.protocolType     , protocolType);
                     sameProto|= 0 == strcasecmp(pair.second.protocolTypeAlias, protocolType);

                if (sameProto && samePow) {
                    return pair.first; //poolImplName
                }
            }
        }
        return "";
    }
}