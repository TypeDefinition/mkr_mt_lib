//
// Created by lnxterry on 12/12/20.
//

#ifndef MKR_MULTITHREAD_LIBRARY_GUARD_HANDLE_H
#define MKR_MULTITHREAD_LIBRARY_GUARD_HANDLE_H

#include <utility>

namespace mkr {
    /**
     * Handle to a threadsafe exclusive read, concurrent write guarded data.
     * @tparam L The typename of the mutex lock.
     * @tparam V The typename of the value.
     * @warning Do not hold on to a guard_handle for long periods.
     * As long as the handle exist, it is LOCKING AND BLOCKING ANOTHER THREAD!
     * DO NOT LOCK THE SAME OBJECT MULTIPLE TIMES AT THE SAME TIME IN THE SAME THREAD.
     * Locking a mutex that the current thread already owns is UNDEFINED BEHAVIOUR.
     * If the guard_handle exists in the same scope as the rw_guard it is from, call release() before they both
     * go out of scope. Because there is a chance that the rw_guard gets deleted before guard_handle, and guard_handle will
     * try to release a deleted mutex, which is UNDEFINED BEHAVIOUR.
     *
     * GOOD:
     * @code
     * void bar(rw_guard<int>* _guarded) {
     *      // The wh will go out of scope and delete BEFORE _guarded.
     *      rw_guard<int>::write_handle wh = _guarded->write_lock();
     *
     *      // Do stuff...
     * }
     *
     * void foo() {
     *     rw_guard<int> guarded;
     *     bar(&guarded);
     * }
     *
     * void baz() {
     *     rw_guard<int> guarded;
     *     rw_guard<int>::write_handle wh = guarded.write_lock();
     *     // The wh releases the mutex lock before it and guarded goes out of scope.
     *     wh.release();
     * }
     * @endcode
     *
     * BAD (Like, REALLY REALLY REALLY REALLY BAD):
     * @code
     * void foo() {
     *     // If guarded is destroyed before wh when they go out of scope, you're FUCKED!
     *     rw_guard<int> guarded;
     *     rw_guard<int>::write_handle wh = guarded.write_lock();
     * }
     *
     * void bar() {
     *     rw_guard<int> guarded;
     *     {
     *         // Locking the same mutex twice in the same thread is UNDEFINED BEHAVIOUR.
     *         rw_guard<int>::write_handle wh = guarded.write_lock();
     *         rw_guard<int>::read_handle rh = guarded.read_lock();
     *     }
     * }
     *
     * class my_class
     * {
     *     rw_guard<int>::write_handle* handle_that_never_dies;
     *
     *     void baz()
     *     {
     *        // You are blocking every other thread out there. Prepare for deadlock.
     *        handle_that_never_dies = &guarded.write_lock();
     *     }
     * }
     * @endcode
     */
    template<typename L, typename V>
    class guard_handle {
    private:
        // Mutex lock.
        L lock_;
        // Value.
        V* value_;

    public:
        /**
         * Constructs the guard_handle.
         * @param _lock The mutex lock.
         * @param _value The value.
         */
        guard_handle(L&& _lock, V* _value)
                :lock_(std::move(_lock)), value_(_value) { }

        /**
         * Move constructor.
         * @param _handle The guard_handle to move from.
         */
        guard_handle(guard_handle&& _handle) noexcept
                :lock_(std::move(_handle.lock_)), value_(_handle.value_) { }

        /**
         * Destructs the guard_handle.
         */
        ~guard_handle() = default;

        guard_handle& operator=(guard_handle&& _handle) noexcept
        {
            lock_ = std::move(_handle.lock_);
            value_ = _handle.value_;
        }

        guard_handle(const guard_handle&) = delete;
        guard_handle& operator=(const guard_handle&) = delete;

        V* operator->() { return value_; }
        V& operator*() { return *value_; }

        const V* operator->() const { return value_; }
        const V& operator*() const { return *value_; }

        /**
         * Checks if this is an empty handle.
         * @return Returns true if this is an empty handle, else return false.
         */
        [[nodiscard]]
        bool empty() const { return value_==nullptr; }

        [[nodiscard]]
        bool has_value() const { return value_!=nullptr; }

        /**
         * Releases the handle. The handle becomes an empty handle after this operation.
         */
        void release()
        {
            if (lock_.owns_lock()) {
                lock_.unlock();
                value_ = nullptr;
            }
        }
    };
}

#endif //MKR_MULTITHREAD_LIBRARY_GUARD_HANDLE_H