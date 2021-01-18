//
// Created by lnxterry on 17/1/21.
//

#ifndef MKR_MULTITHREAD_LIBRARY_EVENT_DISPATCHER_H
#define MKR_MULTITHREAD_LIBRARY_EVENT_DISPATCHER_H

#include "../container/threadsafe_hashtable.h"
#include "../util/category.h"
#include "../util/comparator.h"
#include "event_listener.h"

namespace mkr {
    class event_dispatcher {
    private:
        class listener_list {
        private:
            std::mutex mutex_;
            threadsafe_list<event_listener*> list_;

        public:
            listener_list() = default;

            void add_listener(event_listener* _listener)
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (list_.match_none(comparator<event_listener*>{_listener})) {
                    list_.push_front(_listener);
                }
            }

            void remove_listener(event_listener* _listener)
            {
                list_.remove_if(comparator<event_listener*>{_listener});
            }

            void dispatch_event(const event* _event)
            {
                list_.write_each<void>(
                        [_event](event_listener* _listener) -> void {
                            _listener->invoke_callback(_event);
                        });
            }
        };

        typedef std::shared_ptr<listener_list> list_ptr;
        typedef std::shared_ptr<list_ptr> list_ptr_ptr;

        threadsafe_hashtable<category_id, list_ptr> listeners_;

    public:
        event_dispatcher() { }
        ~event_dispatcher() { }

        template<class Event>
        void add_listener(event_listener& _listener)
        {
            list_ptr_ptr lpp = listeners_.get_or_insert(CATEGORY_ID(event, Event),
                    std::make_shared<listener_list>);
            (*lpp)->add_listener(&_listener);
        }

        template<class Event>
        void remove_listener(event_listener& _listener)
        {
            list_ptr_ptr lpp = listeners_.get(CATEGORY_ID(event, Event));
            if (lpp) {
                (*lpp)->remove_listener(&_listener);
            }
        }

        template<class Event>
        void dispatch_event(const Event* _event)
        {
            list_ptr_ptr lpp = listeners_.get(CATEGORY_ID(event, Event));
            if (lpp) {
                (*lpp)->dispatch_event(_event);
            }
        }
    };
}

#endif //MKR_MULTITHREAD_LIBRARY_EVENT_DISPATCHER_H