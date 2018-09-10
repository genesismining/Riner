
#pragma once

#include <memory>

namespace miner {

    template<class T, class DeleterT = std::default_delete<T>>
    using unique_ptr = std::unique_ptr<T, DeleterT>;

}

