#pragma once

namespace mkr {
    /// A base class for events.
    class event {
    public:
        event() = default;
        virtual ~event() = default;
    };
}