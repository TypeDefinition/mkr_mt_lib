//
// Created by lnxterry on 16/1/21.
//

#ifndef MKR_MULTITHREAD_LIBRARY_EVENT_H
#define MKR_MULTITHREAD_LIBRARY_EVENT_H

namespace mkr {
    /// A base class for events.
    class event {
    public:
        /// Constructs the event.
        event() { }
        /// Destructs the event.
        virtual ~event() { }
    };
}

#endif //MKR_MULTITHREAD_LIBRARY_EVENT_H