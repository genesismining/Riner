
#include <iostream>

#include "Application.h"
#include <src/common/OpenCL.h>

#include <easylogging++.h>
INITIALIZE_EASYLOGGINGPP

namespace miner {

    Application::Application(int argc, char *argv[]) {
        START_EASYLOGGINGPP(argc, argv);

        LOG(INFO) << "logging works";
    }

}









