//
// Created by lnxterry on 16/1/21.
//

#ifndef MKR_MULTITHREAD_LIBRARY_EVENT_LISTENER_H
#define MKR_MULTITHREAD_LIBRARY_EVENT_LISTENER_H

#include "event.h"

#include <functional>
#include <utility>

namespace mkr {
    class event_listener {
    private:
        const std::function<void(const event*)> callback_;

    public:
        event_listener(std::function<void(const event*)> _callback)
                :callback_{std::move(_callback)} { }
        ~event_listener() { }

        void invoke_callback(const event* _event)
        {
            if (callback_) { callback_(_event); }
        }
    };
}

#endif //MKR_MULTITHREAD_LIBRARY_EVENT_LISTENER_H