#pragma once

#include <easylogging++.h>

namespace miner {

    template<class ... Args>
    std::string printfStr(const char *format, Args &&... args) {
        std::vector<char> vec(128);
        int64_t size_needed = 128;
        while (true) {
            size_needed = snprintf(vec.data(), vec.size(), format, std::forward<Args>(args)...);
            if (size_needed < 0)
                return std::string("encoding error in format string: '") + format + "'";
            if (size_needed < vec.size())
                return vec.data();
            vec.resize(size_t(size_needed));
        }
    }

}