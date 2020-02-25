#include <src/util/Logging.h>

#include <easylogging++.cc>
INITIALIZE_EASYLOGGINGPP

namespace miner {

    namespace {
        struct {
            bool colorEnabled = false;
            bool emojisEnabled = false;
        } g_log_extras;
    }

    void setThreadName(const std::string &name) {
        el::Helpers::setThreadName(name);
    }

    void setThreadName(const std::stringstream &sstream) {
        setThreadName(sstream.str());
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

        std::string fmt  = "%datetime %level [%thread] %msg"; //if you change this, make sure to also adjust the fancified log (see void fancifyLog(std::string &msg))
        std::string fmtv = "%datetime %level%vlevel[%thread] %msg"; //for verbose

        c.setGlobally(CT::Format, fmt);
        c.set(Level::Verbose, CT::Format, fmtv);
        //c.set(Level::Verbose, CT::Enabled, "false");
        Loggers::reconfigureLogger("default", c);

        Loggers::setVerboseLevel(0);
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
                        {"VERBOSE", "V", "➰", GRAY},
                        {"INFO"   , "I", "➖", RESET},
                        {"DEBUG"  , "D", "🌀", BLUE},
                        {"WARNING", "W", "⚠️ ", ORANGE},
                        {"ERROR"  , "E", "❌", RED},
                        {"FATAL"  , "F", "☣️", REDBOLD}
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