//
// Created by lnxterry on 10/12/20.
//

#ifndef MKR_MULTITHREAD_LIBRARY_RW_GUARD_H
#define MKR_MULTITHREAD_LIBRARY_RW_GUARD_H

#include "guard_handle.h"

#include <memory>
#include <shared_mutex>
#include <functional>

namespace mkr {
    /**
     * Threadsafe exclusive read, concurrent write guard.
     *
     * Addtional Requirements:
     * - rw_guard must be able to support type T where T is a non-copyable or non-movable type.
     * - rw_guard does not have to support a non-copyable AND non-movable type.
     *
     * @tparam T The typename of the contained value.
     */
    template<typename T>
    class rw_guard {
    private:
        typedef std::shared_timed_mutex mutex_type;
        typedef std::unique_lock<mutex_type> writer_lock;
        typedef std::shared_lock<mutex_type> reader_lock;

        mutable mutex_type value_mutex_;
        std::unique_ptr<T> value_;

    public:
        typedef guard_handle<writer_lock, T> write_handle;
        typedef guard_handle<reader_lock, const T> read_handle;

        /**
         * Constructs the rw_guard.
         * @tparam Args Typename of the parameters of T's constructor.
         * @param _args Parameters of T's constructor.
         */
        template<typename... Args>
        explicit rw_guard(Args&& ... _args)
                : value_(std::make_unique<T>(std::forward<Args>(_args)...)) { }

        /**
         * Destructs the rw_guard.
         */
        ~rw_guard() { }

        rw_guard(const rw_guard&) = delete;
        rw_guard(rw_guard&&) = delete;
        rw_guard& operator=(const rw_guard&) = delete;
        rw_guard& operator=(rw_guard&&) = delete;

        /**
         * Lock the data for writing.
         * @return A write_handle for writing to the value.
         * @warning Avoid obtaining multiple locks at the same time, or holding on to locks for long periods of time.
         *          Locks are BLOCKING and may stall other threads until the lock is released.
         *          It is also dangerous to obtain multiple locks on the same thread. If the same thread was to lock a mutex
         *          it already owned again, it causes UNDEFINED BEHAVIOUR.
         */
        [[nodiscard]]
        write_handle write_lock()
        {
            return write_handle(writer_lock(value_mutex_), value_.get());
        }

        /**
         * Try to lock the data for writing.
         * @return A write_handle for writing to the value. If another thread already locked the data, an empty handle is returned.
         * @warning Avoid obtaining multiple locks at the same time, or holding on to locks for long periods of time.
         *          Locks are BLOCKING and may stall other threads until the lock is released.
         *          It is also dangerous to obtain multiple locks on the same thread. If the same thread was to lock a mutex
         *          it already owned again, it causes UNDEFINED BEHAVIOUR.
         */
        [[nodiscard]]
        write_handle try_write_lock()
        {
            writer_lock lock(value_mutex_, std::try_to_lock);
            if (lock.owns_lock()) {
                return write_handle(std::move(lock), value_.get());
            }
            return write_handle(std::move(lock), nullptr);
        }

        /**
         * Lock the data for reading.
         * @return A read_handle for reading from the value.
         * @warning Avoid obtaining multiple locks at the same time, or holding on to locks for long periods of time.
         *          Locks are BLOCKING and may stall other threads until the lock is released.
         *          It is also dangerous to obtain multiple locks on the same thread. If the same thread was to lock a mutex
         *          it already owned again, it causes UNDEFINED BEHAVIOUR.
         */
        [[nodiscard]]
        read_handle read_lock() const
        {
            return read_handle(reader_lock(value_mutex_), value_.get());
        }

        /**
         * Try to lock the data for reading.
         * @return A read_handle for reading from the value. If another thread already locked the data for writing, an empty handle is returned.
         * @warning Avoid obtaining multiple locks at the same time, or holding on to locks for long periods of time.
         *          Locks are BLOCKING and may stall other threads until the lock is released.
         *          It is also dangerous to obtain multiple locks on the same thread. If the same thread was to lock a mutex
         *          it already owned again, it causes UNDEFINED BEHAVIOUR.
         */
        [[nodiscard]]
        read_handle try_read_lock() const
        {
            reader_lock lock(value_mutex_, std::try_to_lock);
            if (lock.owns_lock()) {
                return read_handle(std::move(lock), value_.get());
            }
            return read_handle(std::move(lock), nullptr);
        }

        /**
         * Perform the mapper operation on the value.
         * @tparam return_type The return type of the mapper function.
         * @param _mapper The mapper function to perform on the value.
         * @return The result of the mapper function.
         */
        template<class return_type>
        return_type write_map(const std::function<return_type(T&)>& _mapper)
        {
            writer_lock lock(value_mutex_);
            return _mapper(*value_);
        }

        /**
         * Perform the mapper operation on the value.
         * @tparam return_type The return type of the mapper function.
         * @param _mapper The mapper function to perform on the value.
         * @return The result of the mapper function.
         */
        template<class return_type>
        return_type read_map(const std::function<return_type(const T&)>& _mapper) const
        {
            reader_lock lock(value_mutex_);
            return _mapper(*value_);
        }
    };
}

#endif //MKR_MULTITHREAD_LIBRARY_RW_GUARD_H
