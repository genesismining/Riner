#pragma once

#include <iostream>
#include <sstream>
#define ELPP_CUSTOM_COUT std::cout
#define ELPP_CUSTOM_COUT_LINE(msg) riner::fancifyLog(msg) << std::flush //TODO: consider only flushing on non-verbose logs or for every nth log
#include <easylogging++.h>

namespace riner {

    //implemented in LoggingMain.cpp
    /**
     * sets the calling thread's name to `name`, which will be visible in every log message
     * coming from that thread. It is encouraged you set the thread name in every single
     * thread you created.
     * If the thread name must be generated with e.g a number like "worker 1", "worker 2"
     * there is a SetThreadNameStream helper struct below (which is generally superior to
     * calling setThreadName directly)
     */
    void setThreadName(const std::string &name);

    /**
     * use a stringstream as a convenient setThreadName alternative
     * usage:
     * SetThreadNameStream{} << "MyWorkerThread #" << number;
     */
    struct SetThreadNameStream : public std::stringstream {
        ~SetThreadNameStream() override {setThreadName(str());}
    };

    /**
     * return the thread name as specified by setThreadName or SetThreadNameStream or an easylogging
     * thread id if no name was set.
     */
    std::string getThreadName();

    /**
     * initializes the logging framework so that thread names are shown etc...
     * should be called from within every main function of every target, or the test-main in unit tests
     * param argc argument count from main function
     * param argv argument c-string pointers from main function
     * param color if the log output should be colored via adding certain chars (see `fancifyLog()`)
     * param emojis if the log output should use unicode emojis for different log levels (e.g. error is a red X)
     */
    void initLogging(int argc, const char **argv, bool color = false, bool emojis = false);

    /**
     * the call that an easylogging message goes through (via ELPP_CUSTOM_COUT_LINE) that
     * will modify the log message according to the settings previously provided through initLogging
     * param msg the log message before modification (color, emoji, ...)
     * return the log message after modification (color, emoji, ...)
     */
    std::string &fancifyLog(std::string &msg);

    /**
     * sprintf, but returns a properly sized std::string
     * param format the format string (see sprintf)
     * param args the arguments that the format string shoudl be formatted with
     * return format string with added args (see sprintf)
     */
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
