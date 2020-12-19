//
// Created by lnxterry on 12/12/20.
//

#ifndef MKR_MULTITHREAD_LIBRARY_GUARD_HANDLE_H
#define MKR_MULTITHREAD_LIBRARY_GUARD_HANDLE_H

namespace mkr {
    /**
     * Handle to a threadsafe exclusive read, concurrent write guarded data.
     * @tparam L The typename of the mutex lock.
     * @tparam V The typename of the value.
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