
#pragma once

#include <type_safe/variant.hpp>

namespace miner {

    namespace ts = type_safe;

    template<class ... Ts>
    using variant = type_safe::variant<Ts...>;

}
