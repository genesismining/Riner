#pragma once

#include <iostream>
#include <sstream>
#define ELPP_CUSTOM_COUT std::cout
#define ELPP_CUSTOM_COUT_LINE(msg) riner::fancifyLog(msg)
#include <easylogging++.h>

namespace riner {

    //implemented in LoggingMain.cpp
    void setThreadName(const std::string &name);

    //use a stringstream as a convenient setThreadName alternative
    //usage:
    //SetThreadNameStream{} << "string " << number;
    struct SetThreadNameStream : public std::stringstream {
        ~SetThreadNameStream() override {setThreadName(str());}
    };

    std::string getThreadName();

    void initLogging(int argc, const char **argv, bool color = false, bool emojis = false);
    bool fancyLogEnabled();
    std::string &fancifyLog(std::string &msg);

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