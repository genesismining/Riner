#include <src/util/Logging.h>

INITIALIZE_EASYLOGGINGPP

namespace miner {

    void setThreadName(const std::string &name) {
        el::Helpers::setThreadName(name);
    }

    void setThreadName(const std::stringstream &sstream) {
        setThreadName(sstream.str());
    }

    std::string getThreadName() {
        return el::Helpers::getThreadName();
    }

    void initLogging(int argc, const char **argv) {
        START_EASYLOGGINGPP(argc, argv);

        using namespace el;
        using CT = ConfigurationType;

        Configurations c;
        c.setToDefault();

        std::string fmt = "%datetime %levshort [%thread] %msg";
#ifndef NDEBUG
        fmt = "%datetime{%s,%g} %levshort [%thread] %msg";
#endif

        c.set(Level::Debug, CT::Format, fmt);
        c.setGlobally(CT::Format, fmt);
        c.set(Level::Verbose, CT::Enabled, "false");
        Loggers::reconfigureLogger("default", c);

        Loggers::setVerboseLevel(0);
    }

}