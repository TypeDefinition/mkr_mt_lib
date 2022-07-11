#pragma once

#include <functional>
#include <utility>
#include "event.h"

namespace mkr {
    /**
     * An event listener. It is used to subscribe to event dispatchers to listen for events.
     */
    class event_listener {
    private:
        /// The callback function to invoke when an event is sent.
        const std::function<void(const event*)> callback_;

    public:
        /**
         * Constructs the event listener.
         * @param _callback The callback function to invoke when an event is sent.
         */
        event_listener(std::function<void(const event*)> _callback)
                : callback_{std::move(_callback)} { }
        /**
         * Destructs the event.
         */
        ~event_listener() = default;

        /**
         * Invoke the callback.
         * @param _event The event that is sent.
         */
        void invoke_callback(const event* _event)
        {
            if (callback_) { callback_(_event); }
        }
    };
}