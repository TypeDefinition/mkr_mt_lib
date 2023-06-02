#pragma once

namespace mkr {
    /// A base class for events.
    class event {
    public:
        /// Constructs the event.
        event() {}

        /// Destructs the event.
        virtual ~event() {}
    };
}