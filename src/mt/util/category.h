#pragma once

#include <atomic>
#include <type_traits>

namespace mkr {
    typedef uint64_t category_id;

    /**
     * Utility class to generate a unique id for a group of classes.
     * Each child class of Base will have a type id generated starting from 0.
     * @tparam Base The base class of the category.
     */
    template<typename Base>
    class category {
    private:
        static std::atomic<category_id> current_id_;

        /**
         * Generate the type id of a class. This exists so that get_id() can remove the cv of the template class.
         * @tparam Derived The class type. Derived may also be of type Base. This type is cv removed.
         * @return The type id of a class
         */
        template<typename Derived>
        static category_id generate_id()
        {
            static category_id id = current_id_++;
            return id;
        }

    public:
        category() = delete;

        /**
         * Get the type id of a class.
         * @tparam Derived The class type. Derived may also be of type Base.
         * @return The type if of Derived.
         */
        template<typename Derived>
        static category_id get_id() requires std::is_base_of_v<Base, Derived>
        {
            return generate_id<std::remove_cvref_t<Derived>>();
        }
    };

    template<typename Base>
    std::atomic<category_id> category<Base>::current_id_ = 0;

#define CATEGORY_ID(__BASE__, __DERIVED__) category<__BASE__>::get_id< __DERIVED__>()
}