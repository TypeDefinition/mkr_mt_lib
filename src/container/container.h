//
// Created by lnxterry on 6/2/21.
//

#ifndef MKR_MULTITHREAD_LIBRARY_CONTAINER_H
#define MKR_MULTITHREAD_LIBRARY_CONTAINER_H

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

#endif //MKR_MULTITHREAD_LIBRARY_CONTAINER_H