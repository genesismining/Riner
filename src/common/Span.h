#pragma once

#include <gsl/span>

namespace miner {
    
    template<class T>
    using span = gsl::span<T>;
    
}