#pragma once

#include "common/family.h"
#include "common/comparators.h"
#include "mt/container/threadsafe_hashtable.h"
#include "mt/container/threadsafe_list.h"
#include "event_listener.h"

namespace mkr {
    /**
     * Event dispatcher used to dispatch events to subscribed listeners.
     */
    class event_dispatcher {
    private:
        typedef threadsafe_list<event_listener*> listener_list;

        /**
         * The list of event listeners subscribed to this dispatcher.
         */
        threadsafe_hashtable<category_id, listener_list, 251> listeners_;

    public:
        /**
         * Constructs the event dispatcher.
         */
        event_dispatcher() = default;
        /**
         * Destructs the event dispatcher.
         */
        ~event_dispatcher() = default;

        /**
         * Subscribes a listener to this dispatcher for this type of event.
         * A listener can subscribe multiple times.
         * A listener can subscribe to multiple events from the same dispatcher.
         * A listener can subscribe to multiple events from different dispatchers.
         * A listener can subscribe to the same event from different dispatchers.
         * A listener can subscribe to the same event from the same dispatcher multiple times.
         * @tparam Event The type of event to listen for. The listener's callback will be invoked when events of this type is dispatched.
         * @param _listener The subscribing listener.
         * @warning The listener needs to unsubscribe as many times as it subscribes.
         */
        template<class Event>
        void subscribe_listener(event_listener& _listener)
        {
            std::shared_ptr<listener_list> lp = listeners_.get_or_insert(CATEGORY_ID(event, Event),
                    []() { return listener_list{}; });

            lp->push_front(&_listener);
        }

        /**
         * Unsubscribes a listener to this dispatcher for this type of event.
         * A listener can subscribe multiple times.
         * A listener can subscribe to multiple events from the same dispatcher.
         * A listener can subscribe to multiple events from different dispatchers.
         * A listener can subscribe to the same event from different dispatchers.
         * A listener can subscribe to the same event from the same dispatcher multiple times.
         * @tparam Event The type of event to listen for. The listener's callback will be invoked when events of this type is dispatched.
         * @param _listener The unsubscribing listener.
         * @warning The listener needs to unsubscribe as many times as it subscribes.
         */
        template<class Event>
        void unsubscribe_listener(event_listener& _listener)
        {
            // Use get instead of map, since map will lock the mutex of the value.
            // But since our value is a threadsafe_list, it is already threadsafe and does not need a mutex.
            std::shared_ptr<listener_list> lp = listeners_.get(CATEGORY_ID(event, Event));
            if (!lp) { return; }

            lp->remove_if(is_equal<event_listener*>{&_listener}, 1);
        }

        /**
         * Dispatches an event to all listeners that are listening for this type of event.
         * @tparam Event The type of event.
         * @param _event The event.
         */
        template<class Event>
        void dispatch_event(const Event* _event)
        {
            std::shared_ptr<listener_list> lp = listeners_.get(CATEGORY_ID(event, Event));
            if (!lp) { return; }

            lp->write_each([_event](event_listener* _listener) -> void {
                _listener->invoke_callback(_event);
            });
        }
    };
}