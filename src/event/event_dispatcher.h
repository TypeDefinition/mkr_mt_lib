//
// Created by lnxterry on 17/1/21.
//

#ifndef MKR_MULTITHREAD_LIBRARY_EVENT_DISPATCHER_H
#define MKR_MULTITHREAD_LIBRARY_EVENT_DISPATCHER_H

#include "event_listener.h"
#include "../container/threadsafe_hashtable.h"
#include "../container/threadsafe_list.h"
#include "../util/category.h"
#include "../util/comparator.h"

namespace mkr {
    class event_dispatcher {
    private:
        typedef threadsafe_list<event_listener*> listener_list;

        threadsafe_hashtable<category_id, listener_list, 251> listeners_;

    public:
        event_dispatcher() { }
        ~event_dispatcher() { }

        template<class Event>
        void add_listener(event_listener& _listener)
        {
            std::shared_ptr<listener_list> lp = listeners_.get_or_insert(CATEGORY_ID(event, Event),
                    []() { return listener_list{}; });
            lp->push_front(&_listener);
        }

        template<class Event>
        void remove_listener(event_listener& _listener)
        {
            // Use get instead of map, since map will lock the mutex of the value.
            // But since our value is a threadsafe_list, it is already threadsafe and does not need a mutex.
            std::shared_ptr<listener_list> lp = listeners_.get(CATEGORY_ID(event, Event));
            if (!lp) { return; }

            lp->remove_if(comparator<event_listener*>{&_listener}, 1);
        }

        template<class Event>
        void dispatch_event(const Event* _event)
        {
            std::shared_ptr<listener_list> lp = listeners_.get(CATEGORY_ID(event, Event));
            if (!lp) { return; }

            lp->write_each<void>([_event](event_listener* _listener) -> void {
                _listener->invoke_callback(_event);
            });
        }
    };
}

#endif //MKR_MULTITHREAD_LIBRARY_EVENT_DISPATCHER_H