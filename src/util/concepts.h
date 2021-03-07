//
// Created by lnxterry on 22/2/21.
//

#ifndef MKR_MULTITHREAD_LIBRARY_CONCEPTS_H
#define MKR_MULTITHREAD_LIBRARY_CONCEPTS_H

#include <functional>
#include <concepts>

namespace mkr {
    template<class T, class U>
    concept not_same_as = !std::is_same_v<T, U>;

    template<class F, class... Args>
    concept is_predicate = std::predicate<F, Args...>;

    template<class F, class... Args>
    concept is_consumer = requires(F _consumer, Args... _args)
    {
        { std::invoke(_consumer, _args...) };
    };

    template<class F, class R>
    concept is_supplier = requires(F _supplier) {
        { std::invoke(_supplier) } -> std::convertible_to<R>;
    };

    template<class F, class... Args>
    concept is_function = requires(F _function, Args... _args)
    {
        { std::invoke(_function, _args...) } -> mkr::not_same_as<void>;
    };
}

#endif //MKR_MULTITHREAD_LIBRARY_CONCEPTS_H