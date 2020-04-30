
#pragma once

#include <chrono>

namespace riner {
    /**
     * add chrono shorthands here as a controlled alternative to "using namespace std::chrono"
     */
    using clock = std::chrono::steady_clock;
    using std::chrono::hours;
    using std::chrono::seconds;
    using std::chrono::minutes;
    using std::chrono::milliseconds;
}
