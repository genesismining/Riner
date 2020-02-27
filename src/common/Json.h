
#pragma once

#include <nlohmann/json.hpp>

//for forward declarations include "JsonForward.h" instead

namespace riner {

    namespace nl = nlohmann;

    //iterate through a json object's members as {key, value} pairs
    inline nl::json::object_t asPairs(const nl::json::value_type &j) {
        return j.get<nl::json::object_t>();
    }

    class JsonSerializable {
    public:
        virtual nl::json toJson() const = 0;

        virtual ~JsonSerializable() = default;
    };

}
