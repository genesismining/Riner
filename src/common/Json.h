
#pragma once

#include <nlohmann/json.hpp>

//avoid including this (Json.h) file.
//if you can, include "JsonForward.h" instead, which contains
//forward declarations and will reduce compile time.

namespace riner {

    namespace nl = nlohmann;

    /**
     * iterate through a json object's members as {key, value} pairs
     * usage:
     * ```for (auto &pair : asPairs(j)) {
     *     auto &json_key   = pair.first;
     *     auto &json_value = pair.second;
     * }```
     */
    inline nl::json::object_t asPairs(const nl::json::value_type &j) {
        return j.get<nl::json::object_t>();
    }


    /**
     * Interface for objects that can be serialized to a nl::json
     */
    class JsonSerializable {
    public:
        virtual nl::json toJson() const = 0;

        virtual ~JsonSerializable() = default;
    };

}
