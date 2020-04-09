
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

    bool startsWith(const std::string &string, const std::string &prefix);

    //returns the part in the string str before the delimiter. if no delimiter is found, return the entire str
    std::string partBefore(const std::string &delimiter, const std::string &str);

    //quick and easy string creation stream shorthand
    //usage (MakeStr{} << "foo" << 4.f << "\n").str()
    struct MakeStr {
        std::stringstream ss {};
        MakeStr() = default;
        MakeStr(MakeStr &&) = default;

        template<class T> MakeStr &&operator<<(T &&t) && {
            ss << (std::forward<T>(t));
            return std::move(*this);
        }

        std::string str() {
            return ss.str();
        }

        operator std::string () const {
            return ss.str();
        }
    };

    //concats and maybe adds/removes a slash, tries to behave like std::filesystem would
    // a   | b   |  return
    // foo , bar => foo/bar
    // foo/, bar => foo/bar
    // foo ,/bar => throw std::invalid_argument
    // foo/,/bar => throw std::invalid_argument
    std::string concatPath(const std::string &a, const std::string &b);

    //returns the string up until the last "/" or "\".
    std::string stripDirFromPath(const std::string &);

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
