
#pragma once

#include <src/common/Optional.h>
#include <src/common/Span.h>

namespace miner {

    //functions for accessing container elements that return optional<Reftype> instead of references
    //naming scheme based on std::get_if

    //like std::map::at(key), but returns an optional depending on whether the key exists
    template<class MapT, class KeyT = typename MapT::key_type, class ReturnT = typename MapT::mapped_type>
    opt::optional<ReturnT &> map_at_if(MapT &map, const KeyT &key) {
        auto it = map.find(key);
        bool exists = it != map.end();
        if (exists)
            return it->second;
        else
            return {};
    }

    //like const std::map::at(key), but returns an optional depending on whether the key exists
    template<class MapT, class KeyT, class ReturnT = const typename MapT::mapped_type>
    opt::optional<ReturnT &> map_at_if(const MapT &map, const KeyT &key) {
        return map_at_if<MapT, KeyT, ReturnT>(map, key);
    }

    //like gsl::span::at(index), but returns an optional depending on whether the key exists
    template<class T>
    opt::optional<T &> at_if(span<T> span, size_t index) {
        if (index < span.size()) {
            return span[index];
        }
        return {};
    }

    class BadOptionalAccess : public std::exception {
    public:
        const char *what() const noexcept override {
            return "bad optional access";
        };
    };

    //unwrap an optional<Reftype> and throw BadOptionalAccess if it is nullopt
    template<class T>
    T &unwrap(const opt::optional<T &> &opt) {
        if (opt)
            return opt.value();
        throw BadOptionalAccess();
    }

}
