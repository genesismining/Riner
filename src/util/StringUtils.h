
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

    /**
     * quick and dirty function that splits an address "host:port" at the ":" into two std::strings
     * param str the address in "host:port" format (if no ":" was found, resulting port is empty)
     * return result struct with host and port split apart. If no ":" was found, host contains str and port is empty.
     */
    ParsePoolAddressResult parsePoolAddress(const char *str);

    /**
     * return lowercase version of inStr according to std::tolower
     **/
    std::string toLower(const std::string &inStr);

    /**
     * convenience wrapper around std::stoll
     * return int64_t if parsing `str` as an `int` was successful, `nullopt` otherwise
     **/
    optional<int64_t> strToInt64(const char *str);

    /**
     * return whether the first `prefix.size()` characters in `string` are equal to `prefix`
     */
    bool startsWith(const std::string &string, const std::string &prefix);

    /**
     * return copy of the part in the string `str` before the delimiter. if no delimiter is found, returns the entire `str`
     */
    std::string partBefore(const std::string &delimiter, const std::string &str);

    /**
     * convenience stream-like to create strings in one line
     * usage: `(MakeStr{} << "foo" << 4.f << "\n").str()`
     */
    struct MakeStr {
        std::stringstream ss {};
        MakeStr() = default;
        MakeStr(MakeStr &&) = default;

        /**
         * adds `t` to the result string, see `std::stringstream::operator<<`
         */
        template<class T> MakeStr &&operator<<(T &&t) && {
            ss << (std::forward<T>(t));
            return std::move(*this);
        }

        /**
         * return the result string
         */
        std::string str() {
            return ss.str();
        }

        /**
         * convenience conversion to obtain the result string
         */
        operator std::string () const {
            return ss.str();
        }
    };

    /**
     * attempt at quickly implementing similar behavior to std::filesystem's binary "/" operator.
     * concatenates two path strings and adds the "/" in between acordingly.
     * throws std::invalid_argument if `bar` starts with an "/".
     *
     * ```
     * foo , bar => foo/bar
     * foo/, bar => foo/bar
     * foo ,/bar => throw std::invalid_argument
     * foo/,/bar => throw std::invalid_argument
     * ```
     * return the paths `a` and `b` concatenated
     * */
    std::string concatPath(const std::string &a, const std::string &b);

    /*
     * return the given string up until the last "/" or "\".
     */
    std::string stripDirFromPath(const std::string &);

    /*
     * convenience function for converting containers to a string
     * return a comma separated printable string of the contents of a container
     */
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
