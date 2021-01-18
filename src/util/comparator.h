//
// Created by lnxterry on 18/1/21.
//

#ifndef MKR_MULTITHREAD_LIBRARY_COMPARATOR_H
#define MKR_MULTITHREAD_LIBRARY_COMPARATOR_H

#include <utility>

namespace mkr {
    template<typename T>
    class comparator {
    private:
        const T value_;
    public:
        comparator(const T& _value)
                :value_{_value} { }
        comparator(T&& _value)
                :value_{std::move(_value)} { }
        bool operator()(const T& _value) { return value_==_value; }
        bool operator()(T&& _value) { return value_==_value; }
    };
}

#endif //MKR_MULTITHREAD_LIBRARY_COMPARATOR_H