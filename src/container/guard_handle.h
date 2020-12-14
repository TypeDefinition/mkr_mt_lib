//
// Created by lnxterry on 12/12/20.
//

#ifndef MKR_MULTITHREAD_LIBRARY_GUARD_HANDLE_H
#define MKR_MULTITHREAD_LIBRARY_GUARD_HANDLE_H

namespace mkr {
    /**
     * Handle to a threadsafe exclusive read, concurrent write guarded data.
     * @tparam lock_type The typename of the mutex lock.
     * @tparam value_type The typename of the value.
     */
    template<typename lock_type, typename value_type>
    class guard_handle {
    private:
        // Mutex lock.
        lock_type lock_;
        // Value.
        value_type* value_;

    public:
        /**
         * Constructs the guard_handle.
         * @param _lock The mutex lock.
         * @param _value The value.
         */
        guard_handle(lock_type&& _lock, value_type* _value)
                :lock_(std::move(_lock)), value_(_value) { }

        /**
         * Move constructor.
         * @param _other The guard_handle to move from.
         */
        guard_handle(guard_handle&& _other) noexcept
                :lock_(std::move(_other.lock_)), value_(_other.value_) { }

        /**
         * Destructs the guard_handle.
         */
        ~guard_handle() = default;

        guard_handle& operator=(guard_handle&& _other) noexcept
        {
            lock_ = std::move(_other.lock_);
            value_ = _other.value_;
        }

        guard_handle(const guard_handle&) = delete;
        guard_handle& operator=(const guard_handle&) = delete;

        value_type* operator->() { return value_; }
        value_type& operator*() { return *value_; }

        const value_type* operator->() const { return value_; }
        const value_type& operator*() const { return *value_; }

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