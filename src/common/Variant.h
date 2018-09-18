
#pragma once

#include <type_safe/variant.hpp>

namespace miner {

    template<class ... Ts>
    using variant = type_safe::variant<Ts...>;

}
