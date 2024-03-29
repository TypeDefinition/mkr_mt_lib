#pragma once

#include "threadsafe_list.h"

namespace mkr {
    /**
     * Threadsafe hashtable.
     *
     * Invariants:
     * - The number of buckets do not change.
     * - Each key can only appear once in the hashtable at any given time.
     * - The same key will always be mapped to the same bucket.
     * - Each key can only be mapped to a single value.
     * - A key must have a value.
     * - When iterating from the head_ of a bucket, it will eventually lead to the tail.
     * - If the corresponding bucket does not contain the key, it does not exist.
     * - The same key must always return the same hash.
     *
     * Addtional Requirements:
     * - threadsafe_hashtable must be able to support type V where V is a non-copyable or non-movable type.
     * - threadsafe_hashtable does not have to support a non-copyable AND non-movable type.
     *
     * @tparam K The typename of the key.
     * @tparam V The typename of the value.
     * @tparam N The number of buckets in the hashtable. Prime numbers are highly recommended.
     */
    template<typename K, typename V, std::size_t N = 61>
    class threadsafe_hashtable : public container {
    private:
        typedef std::shared_timed_mutex mutex_type;
        typedef std::unique_lock<mutex_type> writer_lock;
        typedef std::shared_lock<mutex_type> reader_lock;

        /**
         * A pair key-value pair.
         *
         * @warning The implicitly-declared or defaulted move constructor for class T is defined as deleted if any of the following is true:
         *          1. T has non-static data members that cannot be moved (have deleted, inaccessible, or ambiguous move constructors);
         *          2. T has direct or virtual base class that cannot be moved (has deleted, inaccessible, or ambiguous move constructors);
         *          3. T has direct or virtual base class with a deleted or inaccessible destructor;
         *          4. T is a union-like class and has a variant member with non-trivial move constructor.
         */
        struct pair {
        private:
            // Key.
            const K key_;
            // Value.
            std::shared_ptr<V> value_;

        public:
            pair(const K& _key, std::shared_ptr<V> _value)
                    :key_(_key), value_(_value) { }

            bool match_key(const K& _key) const { return key_==_key; }
            const K& get_key() const { return key_; }
            std::shared_ptr<V> get_value() { return value_; }
            std::shared_ptr<const V> get_value() const { return std::const_pointer_cast<const V>(value_); }
        };

        /**
         * Bucket containing a list of pairs.
         */
        struct bucket {
        public:
            mutable mutex_type mutex_;
            threadsafe_list<pair> list_;
        };

        /**
         * Match Key Function Object
         */
        struct match_key {
        private:
            const K& key_;
        public:
            match_key(const K& _key)
                    :key_(_key) { }
            bool operator()(const pair& _pair) const { return _pair.match_key(key_); }
        };

        /**
         * Pair Supplier Function Object
         */
        struct pair_supplier {
        private:
            const K& key_;
            std::shared_ptr<V> value_;
        public:
            pair_supplier(const K& _key, std::shared_ptr<V> _value)
                    :key_(_key), value_(_value) { }
            pair operator()(void) const { return pair(key_, value_); }
        };

        /**
         * @param _key The key to hash to find the bucket.
         * @return The bucket the key belongs to.
         */
        const bucket& get_bucket(const K& _key) const
        {
            size_t hash = std::hash<K>{}(_key);
            return buckets_[hash%N];
        }

        /**
         * @param _key The key to hash to find the bucket.
         * @return The bucket the key belongs to.
         */
        bucket& get_bucket(const K& _key)
        {
            size_t hash = std::hash<K>{}(_key);
            return buckets_[hash%N];
        }

        /**
         * Internal function to add a key-value pair into the hashtable.
         * @param _key The key to add.
         * @param _value The value to add.
         * @return Returns true if key-value pair was added successfully. Returns false if key already exists in the hashtable.
         */
        bool do_insert(const K& _key, std::shared_ptr<V> _value)
        {
            // Lock bucket so that no other threads can add to the bucket at the same time.
            bucket& b = get_bucket(_key);
            writer_lock b_lock(b.mutex_);

            // If the bucket does not contain they key, add the new pair to the bucket and return true.
            if (b.list_.match_none(match_key{_key})) {
                b.list_.push_front(pair{_key, _value});
                ++num_elements_;
                return true;
            }

            // Otherwise, if the bucket already contains the key, return false.
            return false;
        }

        /**
         * Internal function to replace an existing key-value pair in the hashtable.
         * @param _key The key to replace.
         * @param _value The new value to replace the old value.
         * @return Returns true if key-value pair was replaced successfully. Returns false if key does not already exists in the hashtable.
         */
        bool do_replace(const K& _key, std::shared_ptr<V> _value)
        {
            // Lock bucket so that no other threads can add to the bucket at the same time.
            bucket& b = get_bucket(_key);
            writer_lock b_lock(b.mutex_);

            // Return true if the pair was successfully replace, otherwise, return false.
            return b.list_.replace_if(match_key{_key}, pair_supplier{_key, _value});
        }

        /**
         * Internal function to add a key-value pair into the hashtable. If the value already exists, replace the value.
         * @param _key The key to insert or replace.
         * @param _value The new value to insert or replace the old value.
         * @return Returns true if the operation was successful.
         */
        bool do_insert_or_replace(const K& _key, std::shared_ptr<V> _value)
        {
            // Lock bucket so that no other threads can add to the bucket at the same time.
            bucket& b = get_bucket(_key);
            writer_lock b_lock(b.mutex_);

            // If the bucket already contains the key, replace the value.
            if (!b.list_.replace_if(match_key{_key}, pair_supplier{_key, _value})) {
                // Otherwise, add the key-value pair to the bucket.
                b.list_.push_front(pair{_key, _value});
                ++num_elements_;
            }

            return true;
        }

        /**
         * Constructs a copy of another threadsafe_hashtable. The contents of the other threadsafe_hashtable is copied.
         * @param _threadsafe_hashtable The threadsafe_hashtable to copy.
         */
        void do_copy_construct(const threadsafe_hashtable* _threadsafe_hashtable)
            requires std::copyable<V>
        {
            std::function<void(const K&, const V&)> copy_func =
                    [this](const K& _key, const V& _value) {
                        this->insert(_key, _value);
                    };
            _threadsafe_hashtable->read_each(copy_func);
        }

        /// Buckets
        bucket buckets_[N];
        /// Number of elements in the container.
        std::atomic_size_t num_elements_;

    public:
        /**
         * Constructs the hashtable.
         */
        threadsafe_hashtable()
                :num_elements_(0) { }

        /**
         * Copy constructor. There is no guarantee that the order of elements is preserved.
         * @param _threadsafe_hashtable The hashtable to copy.
         */
        threadsafe_hashtable(const threadsafe_hashtable& _threadsafe_hashtable)
        {
            do_copy_construct(&_threadsafe_hashtable);
        }

        /**
         * Move constructor. There is no guarantee that the order of elements is preserved.
         * @param _threadsafe_hashtable The hashtable to copy.
         */
        threadsafe_hashtable(threadsafe_hashtable&& _threadsafe_hashtable)
        {
            do_copy_construct(&_threadsafe_hashtable);
        }

        /**
         * Destructs the hashtable.
         */
        virtual ~threadsafe_hashtable() { }

        threadsafe_hashtable operator=(const threadsafe_hashtable&) = delete;
        threadsafe_hashtable operator=(threadsafe_hashtable&&) = delete;

        /**
         * Add a key-value pair into the hashtable.
         * @param _key The key to add.
         * @param _value The value to add.
         * @return Returns true if key-value pair was added successfully. Returns false if key already exists in the hashtable.
         */
        bool insert(const K& _key, const V& _value)
        {
            return do_insert(_key, std::make_shared<V>(_value));
        }

        /**
         * Add a key-value pair into the hashtable.
         * @param _key The key to add.
         * @param _value The value to add.
         * @return Returns true if key-value pair was added successfully. Returns false if key already exists in the hashtable.
         */
        bool insert(const K& _key, V&& _value)
        {
            return do_insert(_key, std::make_shared<V>(std::forward<V>(_value)));
        }

        /**
         * Replace an existing key-value pair in the hashtable.
         * @param _key The key of the pair to replace.
         * @param _value The new value to replace the old value.
         * @return Returns true if key-value pair was replaced successfully. Returns false if key does not already exists in the hashtable.
         */
        bool replace(const K& _key, const V& _value)
        {
            return do_replace(_key, std::make_shared<V>(_value));
        }

        /**
         * Replace an existing key-value pair in the hashtable.
         * @param _key The key of the pair to replace.
         * @param _value The new value to replace the old value.
         * @return Returns true if key-value pair was replaced successfully. Returns false if key does not already exists in the hashtable.
         */
        bool replace(const K& _key, V&& _value)
        {
            return do_replace(_key, std::make_shared<V>(std::forward<V>(_value)));
        }

        /**
         * Add a key-value pair into the hashtable. If the value already exists, replace the value.
         * @param _key The key to insert or replace.
         * @param _value The new value to insert or replace the old value.
         * @return Returns true if the operation was successful.
         */
        bool insert_or_replace(const K& _key, const V& _value)
        {
            return do_insert_or_replace(_key, std::make_shared<V>(_value));
        }

        /**
         * Add a key-value pair into the hashtable. If the value already exists, replace the value.
         * @param _key The key to insert or replace.
         * @param _value The new value to insert or replace the old value.
         * @return Returns true if the operation was successful.
         */
        bool insert_or_replace(const K& _key, V&& _value)
        {
            return do_insert_or_replace(_key, std::make_shared<V>(std::forward<V>(_value)));
        }

        /**
         * Remove an existing key-value pair in the hashtable.
         * @param _key The key of the pair to remove.
         * @return Returns true if key-value pair was removed successfully. Returns false if key does not already exists in the hashtable.
         */
        bool remove(const K& _key)
        {
            // Step 2: Lock bucket
            bucket& b = get_bucket(_key);
            writer_lock b_lock(b.mutex_);

            if (b.list_.remove_if(match_key{_key})) {
                --num_elements_;
                return true;
            }
            return false;
        }

        /**
         * Returns the value that belongs to the key.
         * @param _key The key that the value belongs to.
         * @return Returns the value that belongs to the key. If the key does not exist in the hashtable, nullptr is returned.
         */
        std::shared_ptr<V> get(const K& _key)
        {
            bucket& b = get_bucket(_key);
            reader_lock b_lock(b.mutex_);

            std::shared_ptr<pair> p = b.list_.find_first_if(match_key{_key});
            return p ? p->get_value() : nullptr;
        }

        /**
         * Returns the value that belongs to the key.
         * @param _key The key that the value belongs to.
         * @return Returns the value that belongs to the key. If the key does not exist in the hashtable, nullptr is returned.
         */
        std::shared_ptr<const V> get(const K& _key) const
        {
            const bucket& b = get_bucket(_key);
            reader_lock b_lock(b.mutex_);

            std::shared_ptr<const pair> p = b.list_.find_first_if(match_key{_key});
            return p ? p->get_value() : nullptr;
        }

         /**
          * Returns the value that belongs to the key if it exists. Else, insert a new value and return that.
          * @tparam Supplier The typename of the supplier function.
          * @param _key The key that the value belongs to.
          * @param _supplier A supplier to construct a new value if there is no existing one.
          * @return The value that belongs to the key if it exists. Else, insert a new value and return that.
          */
        template<class Supplier>
        std::shared_ptr<V> get_or_insert(const K& _key, Supplier&& _supplier)
            requires mkr::is_supplier<Supplier, V>
        {
            // Check if there is already an existing value.
            std::shared_ptr<V> existing_value = get(_key);
            if (existing_value) { return existing_value; }

            // Lock bucket so that no other threads can add to the bucket at the same time.
            bucket& b = get_bucket(_key);
            // If the value does not exist, we need to write-lock.
            writer_lock b_lock(b.mutex_);

            // We need to check if the value exists again, in case it was added between our first check and now.
            std::shared_ptr<pair> p = b.list_.find_first_if(match_key{_key});
            if (p) { return p->get_value(); }

            std::shared_ptr<V> new_value = std::make_shared<V>(std::invoke(std::forward<Supplier>(_supplier)));
            b.list_.push_front(pair{_key, new_value});
            ++num_elements_;
            return new_value;
        }

         /**
          * Perform a mapping operation on a value specified by a key. If the key does not exists, a std::nullopt is returned.
          * write_and_map allows modifying of the value in the threadsafe_hashtable.
          * @tparam Mapper The type of mapper function. Takes in the value as a parameter.
          * @param _key The key of the value.
          * @param _mapper The mapper function.
          * @return An std::optional containing the return value of the mapper function if the key exists. Else, returns a std::nullopt.
          */
        template<typename Mapper>
        std::optional<std::invoke_result_t<Mapper, V&>> write_and_map(const K& _key, Mapper&& _mapper)
            requires mkr::is_function<Mapper, V&>
        {
            bucket& b = get_bucket(_key);
            writer_lock b_lock(b.mutex_);

            std::shared_ptr<pair> p = b.list_.find_first_if(match_key{_key});
            return p ? std::optional<std::invoke_result_t<Mapper, V&>>{
                    std::invoke(std::forward<Mapper>(_mapper), *p->get_value())} : std::nullopt;
        }

        /**
          * Perform a mapping operation on a value specified by a key. If the key does not exists, a std::nullopt is returned.
          * read_and_map does not allow modifying of the value in the threadsafe_hashtable.
          * @tparam Mapper The type of mapper function. Takes in the value as a parameter.
          * @param _key The key of the value.
          * @param _mapper The mapper function.
          * @return An std::optional containing the return value of the mapper function if the key exists. Else, returns a std::nullopt.
          */
        template<typename Mapper>
        std::optional<std::invoke_result_t<Mapper, const V&>> read_and_map(const K& _key, Mapper&& _mapper) const
            requires mkr::is_function<Mapper, const V&>
        {
            const bucket& b = get_bucket(_key);
            reader_lock b_lock(b.mutex_);

            std::shared_ptr<const pair> p = b.list_.find_first_if(match_key{_key});
            return p ? std::optional<std::invoke_result_t<Mapper, const V&>>{
                    std::invoke(std::forward<Mapper>(_mapper), *p->get_value())} : std::nullopt;
        }

        /**
          * Perform the consumer operation on each value in the hashtable.
          * write_each allows modifying the values on the threadsafe_hashtable.
          * @tparam Consumer The typename of the consumer function.
          * @param _consumer Consumer to operate on the values.
          */
        template<class Consumer>
        void write_each(Consumer&& _consumer)
            requires mkr::is_consumer<Consumer, const K&, V&>
        {
            for (size_t i = 0; i<N; ++i) {
                bucket& b = buckets_[i];
                writer_lock b_lock(b.mutex_);

                std::function<void(pair&)> consumer_wrapper = [consumer = std::forward<Consumer>(_consumer)](
                        pair& _pair) {
                    std::invoke(consumer, _pair.get_key(), *_pair.get_value());
                };
                b.list_.write_each(consumer_wrapper);
            }
        }

        /**
          * Perform the consumer operation on each value in the hashtable.
          * read_each does not allow modifying the values on the threadsafe_hashtable.
          * @tparam Consumer The typename of the consumer function.
          * @param _consumer Consumer to operate on the values.
          */
        template<class Consumer>
        void read_each(Consumer&& _consumer) const
            requires mkr::is_consumer<Consumer, const K&, const V&>
        {
            for (size_t i = 0; i<N; ++i) {
                const bucket& b = buckets_[i];
                reader_lock b_lock(b.mutex_);

                std::function<void(const pair&)> consumer_wrapper = [consumer = std::forward<Consumer>(_consumer)](
                        const pair& _pair) {
                    std::invoke(consumer, _pair.get_key(), *_pair.get_value());
                };
                b.list_.read_each(consumer_wrapper);
            }
        }

        /**
         * Checks if the hashtable contains the key.
         * @param _key The key to check.
         * @return True if the hashtable contains the key, else false.
         */
        bool has(const K& _key) const
        {
            const bucket& b = get_bucket(_key);
            reader_lock b_lock(b.mutex_);

            return b.list_.match_any(match_key{_key});
        }

        /**
         * Clear the hashtable.
         */
        void clear()
        {
            writer_lock b_locks[N];
            for (size_t i = 0; i<N; i++) {
                b_locks[i] = std::move(writer_lock(buckets_[i].mutex_));
                buckets_[i].list_.clear();
            }
            num_elements_ = 0;
        }

        /**
         * Checks if the container is empty.
         * @return Returns true if the container is empty, false otherwise.
         */
        bool empty() const { return num_elements_.load()==0; }

        /**
         * Returns the number of elements in the container.
         * @return Returns the number of elements in the container.
         */
        std::size_t size() const { return num_elements_.load(); }
    };
}