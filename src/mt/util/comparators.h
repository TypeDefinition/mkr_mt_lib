#pragma once

#include <utility>

namespace mkr {
    /**
     * Helper functor class used for equality comparison.
     * @tparam T The type of the object to compare.
     */
    template<typename T>
    class is_equal {
    private:
        const T value_;
    public:
        is_equal(const T& _value)
                :value_{_value} { }
        is_equal(T&& _value)
                :value_{std::move(_value)} { }
        bool operator()(const T& _value) { return value_==_value; }
        bool operator()(T&& _value) { return value_==_value; }
    };

    /**
     * Helper functor class used for lesser than comparison.
     * @tparam T The type of the object to compare.
     */
    template<typename T>
    class is_lesser {
    private:
        const T value_;
    public:
        is_lesser(const T& _value)
                :value_{_value} { }
        is_lesser(T&& _value)
                :value_{std::move(_value)} { }
        bool operator()(const T& _value) { return value_<_value; }
        bool operator()(T&& _value) { return value_<_value; }
    };

    /**
     * Helper functor class used for greater than comparison.
     * @tparam T The type of the object to compare.
     */
    template<typename T>
    class is_greater {
    private:
        const T value_;
    public:
        is_greater(const T& _value)
                :value_{_value} { }
        is_greater(T&& _value)
                :value_{std::move(_value)} { }
        bool operator()(const T& _value) { return _value<value_; }
        bool operator()(T&& _value) { return _value<value_; }
    };

    /**
     * Helper functor class used for lesser than or equal comparison.
     * @tparam T The type of the object to compare.
     */
    template<typename T>
    class is_lesser_or_equal {
    private:
        const T value_;
    public:
        is_lesser_or_equal(const T& _value)
                :value_{_value} { }
        is_lesser_or_equal(T&& _value)
                :value_{std::move(_value)} { }
        bool operator()(const T& _value) { return value_<_value || value_==_value; }
        bool operator()(T&& _value) { return value_<_value || value_==_value; }
    };

    /**
     * Helper functor class used for greater than or equal comparison.
     * @tparam T The type of the object to compare.
     */
    template<typename T>
    class is_greater_or_equal {
    private:
        const T value_;
    public:
        is_greater_or_equal(const T& _value)
                :value_{_value} { }
        is_greater_or_equal(T&& _value)
                :value_{std::move(_value)} { }
        bool operator()(const T& _value) { return _value<value_ || value_==_value; }
        bool operator()(T&& _value) { return _value<value_ || value_==_value; }
    };
}