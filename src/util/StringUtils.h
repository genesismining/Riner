
#pragma once

#include <src/common/Optional.h>

#include <algorithm>
#include <string>
#include <sstream>

namespace miner {

    struct ParsePoolAddressResult {
        std::string host;
        std::string port;
    };

    ParsePoolAddressResult parsePoolAddress(const char *str);

    //returns lowercase version of inStr
    std::string toLower(const std::string &inStr);

    bool startsWith(const std::string& string, const std::string& prefix);

    template<typename C>
    std::string toString(C container) {
        std::ostringstream s;
        if (!container.empty()) {
            std::copy(container.begin(), container.end() - 1, std::ostream_iterator<typename C::value_type>(s, ", "));
            s << container.back();
        }
        return s.str();
    }

} // namespace miner
