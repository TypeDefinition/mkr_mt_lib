//
// Created by lnxterry on 18/1/21.
//

#ifndef MKR_MULTITHREAD_LIBRARY_CATEGORY_H
#define MKR_MULTITHREAD_LIBRARY_CATEGORY_H

#include <atomic>
#include <type_traits>

namespace mkr {
    typedef unsigned long category_id;

    template<typename Base>
    class category {
    private:
        static std::atomic<category_id> current_id_;

        template<typename Derived>
        static category_id generate_id()
        {
            static category_id id = current_id_++;
            return id;
        }

    public:
        category() = delete;

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

#endif //MKR_MULTITHREAD_LIBRARY_CATEGORY_H