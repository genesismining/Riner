#include <src/util/Logging.h>

#include <src/application/CLI.h>
#include <src/util/StringUtils.h>

#include <easylogging++.cc>
INITIALIZE_EASYLOGGINGPP

namespace riner {

    namespace {
        struct {
            bool colorEnabled = false;
            bool emojisEnabled = false;
        } g_log_extras;
    }

    void setThreadName(const std::string &name) {
        el::Helpers::setThreadName(name);
    }

    std::string getThreadName() {
        return el::Helpers::getThreadName();
    }

    void initLogging(int argc, const char **argv, bool color, bool emojis) {
        g_log_extras.colorEnabled  = color;
        g_log_extras.emojisEnabled = emojis;

        START_EASYLOGGINGPP(argc, argv);

        using namespace el;
        using CT = ConfigurationType;

        Configurations c;
        c.setToDefault();

        std::string time_fmt = "%datetime";
        if (hasArg({"--sec"}, argc, argv)) {
            time_fmt = "%datetime{%s,%g}";
        }

        std::string fmt = time_fmt + " %level [%thread] %msg"; //if you change this, make sure to also adjust the fancified log (see void fancifyLog(std::string &msg))
        std::string fmtv = time_fmt + " %level%vlevel [%thread] %msg"; //for verbose

        c.setGlobally(CT::Format, fmt);
        c.set(Level::Verbose, CT::Format, fmtv);

        Loggers::reconfigureLogger("default", c); //reconfigure now, for logs that may happen when failing to parse verbose args, then reconfigure again

        optional<std::string> logLevelStr = getValueAfterArgWithEqualsSign({"-v", "--verbose"}, argc, argv);
        SetThreadNameStream{} << "log init"; //thread has name "log init" to show a non-confusing name when something happens before the thread name is set

        if (hasArg({"-v", "--verbose"}, argc, argv) || logLevelStr.has_value())  {
            c.set(Level::Verbose, CT::Enabled, "true");
            if (logLevelStr)
                if (auto level = strToInt64(logLevelStr->c_str()))
                    if (0 <= *level && *level < 10)
                        Loggers::setVerboseLevel(*level);
        }
        else { // !hasArg
            c.set(Level::Verbose, CT::Enabled, "false");
        }

        Loggers::reconfigureLogger("default", c);
    }

    std::string &fancifyLog(std::string &msg) {
#define RESET  "\033[0m"
#define REDBOLD "\033[31m\033[1m"
#define RED    "\033[31m"
#define ORANGE "\033[33m"
#define GRAY   "\033[90m"
#define BLUE   "\033[34m"

        if (g_log_extras.colorEnabled || g_log_extras.emojisEnabled) {

            size_t prefix_end  = msg.find('['); //prefix ends latest where the thread [ starts
            if (prefix_end != std::string::npos) {

                const char *levels[][4] = {
                        {"VERBOSE", "V", u8"\u27b0", GRAY},
                        {"INFO"   , "I ", u8"\u2796 ", RESET},
                        {"DEBUG"  , "D ", u8"\xF0\x9F\x8C\x80 ", BLUE}, //U+1F300
                        {"WARNING", "W ", u8"\u26a0\ufe0f ", ORANGE},
                        {"ERROR"  , "E ", u8"\u274c ", RED},
                        {"FATAL"  , "F ", u8"\u2623\ufe0f️ ", REDBOLD}
                };

                for (auto &level : levels) {
                    const char
                    *lev  = level[0],
                    *shrt = level[1 + (g_log_extras.emojisEnabled > 0)],
                    *col   = g_log_extras.colorEnabled ? level[3] : "",
                    *reset = g_log_extras.colorEnabled ? RESET    : "";

                    uint64_t lev_beg = msg.substr(0, prefix_end).find(lev);

                    if (lev_beg == std::string::npos)
                        continue;

                    auto lev_end = lev_beg + strlen(lev);


                    msg = std::string(col) + msg.replace(lev_beg, lev_end - lev_beg, shrt);
                    if (!msg.empty() && msg.back() == '\n') { //add the RESET before the \n
                        msg.pop_back();
                        msg = msg + reset + "\n";
                    }
                    else {
                        msg += reset;
                    }

                }
            }
        }
        return msg;
    }

}