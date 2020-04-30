//
//

#pragma once

#include <type_traits>

namespace riner {

    /**
     * SFINAE utility type
     */
    template<class T>
    struct void_ {
        using type = void;
    };

    /**
     * metafunction that returns whether T contains a public T::value_type typedef
     */
    template<class T, class = void>
    struct has_value_type : std::false_type {};

    template<class T>
    struct has_value_type<T, typename void_<typename T::value_type>::type>
            : std::true_type {};

    /**
     * metafunction that returns whether T contains a public T::value_type typedef
     */
    template<class T>
    constexpr bool has_value_type_v = has_value_type<T>::value;

}
