#pragma once

namespace mkr {
    /**
     * Base class for all containers in this library.
     */
    class container {
    public:
        /**
         * Constructs the container.
         */
        container() { }
        /**
         * Destructs the container.
         */
        virtual ~container() { }
    };
}