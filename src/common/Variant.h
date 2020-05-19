
#pragma once

#include <variant.hpp>

namespace riner {

    namespace var = mpark;

    /**
     * provide a function `func` that gets called with the `var`'s contents if they matches the type `T`
     * usage:
     * ```
     * variant<int, Foo> my_variant;
     * //...
     * visit<Foo>(my_variant, [&] (Foo &foo) {
     *     foo.doSomething(); //only gets called if var contains a Foo.
     * }
     * ```
     * @param var the variant which may contain a `T`
     * @param func a function that takes an argument that can bind to a `T &` or `const T &` dependent on `var`'s type
     * @return whether the function was called (useful for trying multiple types and deciding when to stop)
     */
    template<class T, class Variant, class Func>
    bool visit(Variant &var, Func &&func) {
        if (auto ptr = var::get_if<T>(&var)) {
            func(*ptr);
            return true;
        }
        return false;
    }

    /**
     * convenience typedef for variant
     */
    template<class... T>
    using variant = var::variant<T...>;

}
