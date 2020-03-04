
#pragma once

#include <src/common/Optional.h>

#include <algorithm>
#include <string>
#include <sstream>
#include <iterator>

namespace riner {

    struct ParsePoolAddressResult {
        std::string host;
        std::string port;
    };

    ParsePoolAddressResult parsePoolAddress(const char *str);

    //returns lowercase version of inStr
    std::string toLower(const std::string &inStr);

    optional<int64_t> strToInt64(const char *str);

    bool startsWith(const std::string& string, const std::string& prefix);

    //concats and maybe adds/removes a slash, tries to behave like std::filesystem would
    // a   | b   |  return
    // foo , bar => foo/bar
    // foo/, bar => foo/bar
    // foo ,/bar => throw std::invalid_argument
    // foo/,/bar => throw std::invalid_argument
    std::string concatPath(const std::string &a, const std::string &b);

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
