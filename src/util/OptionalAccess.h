
#pragma once

#include <src/common/Optional.h>
#include <src/common/Span.h>

namespace riner {

    //functions for accessing container elements that return optional<Reftype> instead of references
    //naming scheme based on std::get_if

    /**
     * like std::map::at(key), but returns nullopt depending on whether the key exists.
     * naming scheme with "_if" suffix inspired by "std::get_if".
     * @param map the map
     * @param key the key to search for
     * @return the found value or nullopt if the provided key does not exist in the map yet
     */
    template<class MapT, class KeyT = typename MapT::key_type, class ReturnT = typename MapT::mapped_type>
    optional<ReturnT &> map_at_if(MapT &map, const KeyT &key) {
        auto it = map.find(key);
        bool exists = it != map.end();
        if (exists)
            return it->second;
        else
            return {};
    }

    /**
     * like const std::map::at(key), but returns an optional depending on whether the key exists
     * This is just a const version of the function above
     */
    template<class MapT, class KeyT, class ReturnT = const typename MapT::mapped_type>
    optional<ReturnT &> map_at_if(const MapT &map, const KeyT &key) {
        return map_at_if<MapT, KeyT, ReturnT>(map, key);
    }

    /**
     * like gsl::span::at(index), but returns an optional depending on whether the key exists.
     * @param span the span
     * @param index where to look at in span
     * @return span[index] if index is within range, nullopt otherwise
     */
    template<class T>
    optional<T &> at_if(span<T> span, size_t index) {
        if (index < span.size()) {
            return span[index];
        }
        return {};
    }

}
