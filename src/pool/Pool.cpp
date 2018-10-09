
#include "Pool.h"

namespace miner {

    uint64_t WorkProvider::createNewPoolUid() {
        static uint64_t uid = 0;
        ++uid;
        return uid;
    }

}