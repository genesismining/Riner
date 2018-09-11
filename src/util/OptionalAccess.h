
#pragma once

#include <src/common/Optional.h>
#include <src/common/Span.h>

namespace miner {

    //functions for accessing container elements that return optional_refs instead of references
    //naming scheme based on std::get_if

    //like std::map::at(key), but returns an optional depending on whether the key exists
    template<class MapT, class KeyT = typename MapT::key_type, class ReturnT = typename MapT::mapped_type>
    optional_ref<ReturnT> map_at_if(MapT &map, const KeyT &key) {
        auto it = map.find(key);
        bool exists = it != map.end();
        if (exists)
            return optional_ref<ReturnT>(it->second);
        else
            return {nullopt};
    }

    //like const std::map::at(key), but returns an optional depending on whether the key exists
    template<class MapT, class KeyT, class ReturnT = const typename MapT::mapped_type>
    optional_ref<ReturnT> map_at_if(const MapT &map, const KeyT &key) {
        return map_at_if<MapT, KeyT, ReturnT>(map, key);
    }

    //like std::span::at(index), but returns an optional depending on whether the key exists
    template<class T>
    optional_ref<T> at_if(span<T> span, size_t index) {
        if (index < span.size()) {
            return span[index];
        }
        return nullopt;
    }

    class BadOptionalAccess : public std::exception {
    public:
        const char *what() const noexcept override {
            return "bad optional access";
        };
    };

    //unwrap an optional_ref and throw BadOptionalAccess if it is nullopt
    template<class T>
    T &unwrap(const optional_ref<T> &opt) {
        if (opt)
            return opt.value();
        throw BadOptionalAccess();
    }

}
