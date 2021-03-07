//
// Created by lnxterry on 22/2/21.
//

#ifndef MKR_MULTITHREAD_LIBRARY_CONCEPTS_H
#define MKR_MULTITHREAD_LIBRARY_CONCEPTS_H

#include <functional>
#include <concepts>

namespace mkr {
    template<class Predicate, class... Args>
    concept is_predicate = std::predicate<Predicate, Args...>;

    template<class Consumer, class... Args>
    concept is_consumer = requires(Consumer _consumer, Args... _args)
    {
        { std::invoke(_consumer, _args...) };
    };

    template<class Supplier, class R>
    concept is_supplier = requires(Supplier _supplier) {
        { std::invoke(_supplier) } -> std::convertible_to<R>;
    };

    template<class Function, class... Args, class R>
    concept is_function = requires(Function _function, Args... _args)
    {
        { std::invoke(_function, _args...) } -> std::convertible_to<R>;
    };
}

#endif //MKR_MULTITHREAD_LIBRARY_CONCEPTS_H