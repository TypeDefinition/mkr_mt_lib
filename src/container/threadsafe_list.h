//
// Created by lnxterry on 10/12/20.
//

#ifndef MKR_MULTITHREAD_LIBRARY_THREADSAFE_LIST_H
#define MKR_MULTITHREAD_LIBRARY_THREADSAFE_LIST_H

#include "guard_handle.h"

#include <memory>
#include <shared_mutex>
#include <atomic>
#include <functional>
#include <optional>

namespace mkr {
    /**
     * Threadsafe list.
     *
     * Invariants:
     * - Traversing head_->next_ will eventually lead to the bottom node.
     * - For each node x in the queue, where x!=head_, x->value_ points to an instance of T.
     * - For each node x in the queue, where x!=tail_, x->next_ points to the next node in the list.
     * - For a node x, x->next_ == null means that it is the last node.
     * - head_->next_ == null means the list is empty.
     *
     * Addtional Requirements:
     * - threadsafe_list must be able to support type T where T is a non-copyable or non-movable type.
     * - threadsafe_list does not have to support a non-copyable AND non-movable type.
     *
     * @tparam T The typename of the contained values.
     */
    template<typename T>
    class threadsafe_list {
    private:
        typedef std::shared_timed_mutex mutex_type;
        typedef std::unique_lock<mutex_type> writer_lock;
        typedef std::shared_lock<mutex_type> reader_lock;

        /**
         * A node containing a value, and the next node in the stack.
         *
         * @warning The implicitly-declared or defaulted move constructor for class T is defined as deleted if any of the following is true:
         *          1. T has non-static data members that cannot be moved (have deleted, inaccessible, or ambiguous move constructors);
         *          2. T has direct or virtual base class that cannot be moved (has deleted, inaccessible, or ambiguous move constructors);
         *          3. T has direct or virtual base class with a deleted or inaccessible destructor;
         *          4. T is a union-like class and has a variant member with non-trivial move constructor.
         */
        struct node {
            // Mutex.
            mutable mutex_type mutex_;
            // Value.
            std::shared_ptr<T> value_;
            // Next node.
            std::unique_ptr<node> next_;
        };

        /**
         * Internal function to add a new value to the front of the list.
         * @param _value The value to add to the front of the list.
         * @warning If multiple threads call push_front at the same time, the value will be inserted but may no longer be at the front of the list.
         */
        void do_push_front(std::shared_ptr<T> _value)
        {
            // Construct a new node.
            std::unique_ptr<node> new_node = std::make_unique<node>();
            // Set the new node's value to the new value.
            new_node->value_ = _value;
            // Lock the head mutex.
            writer_lock lock(head_.mutex_);
            // Set new node's next node to the head's next node.
            new_node->next_ = std::move(head_.next_);
            // Set the head's next node to the new node.
            head_.next_ = std::move(new_node);
            // Increase element count.
            ++num_elements_;
        }

        /// Dummy head node with no value. Nodes containing values start from head_->next_;
        node head_;
        /// Number of elements in the queue.
        std::atomic_size_t num_elements_;

    public:
        /**
         * Constructs the list.
         */
        threadsafe_list()
                :num_elements_(0) { }

        /**
         * Destructs the list.
         */
        ~threadsafe_list() = default;

        threadsafe_list(const threadsafe_list&) = delete;
        threadsafe_list(threadsafe_list&&) = delete;
        threadsafe_list operator=(const threadsafe_list&) = delete;
        threadsafe_list operator=(threadsafe_list&&) = delete;

        /**
         * Checks if any of the values in the list passes the predicate.
         * @param _predicate Predicate to test the values.
         * @return Returns true if any of the values in the list passes the predicate. Else, returns false.
         */
        bool match_any(std::function<bool(const T&)> _predicate) const
        {
            // Get current (head) node.
            const node* current = &head_;
            // Lock current mutex.
            reader_lock current_lock(head_.mutex_);

            // Get next node
            while (current->next_) {
                // If there is a next node, lock next mutex. Else, end.
                reader_lock next_lock(current->next_->mutex_);
                // Check the next node's value against the predicate. If predicate passes, return true.
                if (_predicate(*current->next_->value_)) { return true; }
                // If predicate fails, advance current node and current lock.
                current = current->next_.get();
                current_lock = std::move(next_lock);
            }

            // Return false if none of the values passes the predicate.
            return false;
        }

        /**
         * Checks if none of the values in the list passes the predicate.
         * @param _predicate Predicate to test the values.
         * @return Returns false if any of the values in the list passes the predicate. Else, returns true.
         */
        bool match_none(std::function<bool(const T&)> _predicate) const { return !match_any(_predicate); }

        /**
         * Adds a new value to the front of the list.
         * @param _value The value to add to the front of the list.
         * @warning If multiple threads call push_front at the same time, the value will be inserted but may no longer be at the front of the list.
         */
        void push_front(const T& _value)
        {
            do_push_front(std::make_shared<T>(_value));
        }

        /**
         * Adds a new value to the front of the list.
         * @param _value The value to add to the front of the list.
         * @warning If multiple threads call push_front at the same time, the value will be inserted but may no longer be at the front of the list.
         */
        void push_front(T&& _value)
        {
            do_push_front(std::make_shared<T>(std::forward<T>(_value)));
        }

        /**
         * Remove all values in the list that passes the predicate.
         * @param _predicate Predicate to test if a value should be removed.
         * @return The number of values removed.
         */
        size_t remove_if(std::function<bool(const T&)> _predicate)
        {
            // Remove counter.
            size_t num_removed = 0;
            // Get current (head) node.
            node* current = &head_;
            // Lock current (head) mutex.
            writer_lock current_lock(current->mutex_);

            // Get next node
            while (current->next_) {
                // If there is a next node, lock next mutex. Else, end.
                writer_lock next_lock(current->next_->mutex_);

                // If predicate passes, advance next node, and discard the "old" next node.
                if (_predicate(*current->next_->value_)) {
                    // Point to the node we want to remove so that it does not go out of scope until we are done.
                    std::unique_ptr<node> node_to_remove = std::move(current->next_);
                    // Advance next node.
                    current->next_ = std::move(node_to_remove->next_);
                    // Unlock the mutex before node_to_remove's mutex goes out of scope and is deleted.
                    next_lock.unlock();
                    // Decrease element count.
                    --num_elements_;
                    // Increase remove counter.
                    num_removed++;
                }
                    // If predicate fails, advance current node.
                else {
                    // Advance current node.
                    current = current->next_.get();
                    // Advance current lock.
                    current_lock = std::move(next_lock);
                }
            }

            return num_removed;
        }

        /**
         * Replace the value if it passes the predicate.
         * @param _predicate Predicate to test if a value should be replaced.
         * @param _supplier Supplier to generate the replacement value.
         * @return The number of values replaced.
         */
        size_t replace_if(std::function<bool(const T&)> _predicate, std::function<T(void)> _supplier)
        {
            size_t num_replaced = 0;
            // Get current (head) node.
            node* current = &head_;
            // Lock current mutex.
            writer_lock current_lock(head_.mutex_);

            // Get next node
            while (current->next_) {
                // If there is a next node, lock next mutex. Else, end.
                writer_lock next_lock(current->next_->mutex_);

                // If the predicate passes, replace the value.
                if (_predicate(*current->next_->value_)) {
                    current->next_->value_ = std::make_shared<T>(_supplier());
                    num_replaced++;
                }

                // Advance current node.
                current = current->next_.get();
                // Advance current lock.
                current_lock = std::move(next_lock);
            }

            return num_replaced;
        }

        /**
         * Perform the consumer operation on each value in the list.
         * @param _consumer Consumer to operate on the values.
         */
        void write_each(std::function<void(T&)> _consumer)
        {
            // Get current (head) node.
            node* current = &head_;
            // Lock current mutex.
            writer_lock current_lock(head_.mutex_);

            // Get next node
            while (current->next_) {
                // If there is a next node, lock next mutex. Else, end.
                writer_lock next_lock(current->next_->mutex_);

                // Let the consumer operate on the value.
                _consumer(*current->next_->value_);

                // Advance current node.
                current = current->next_.get();
                // Advance current lock.
                current_lock = std::move(next_lock);
            }
        }

        /**
         * Perform the consumer operation on each value in the list.
         * @param _consumer Consumer to operate on the values.
         */
        void read_each(std::function<void(const T&)> _consumer) const
        {
            // Get current (head) node.
            const node* current = &head_;
            // Lock current mutex.
            reader_lock current_lock(head_.mutex_);

            // Get next node
            while (current->next_) {
                // If there is a next node, lock next mutex. Else, end.
                reader_lock next_lock(current->next_->mutex_);

                // Let the consumer operate on the value.
                _consumer(*current->next_->value_);

                // Advance current node.
                current = current->next_.get();
                // Advance current lock.
                current_lock = std::move(next_lock);
            }
        }

        /**
         * Find and return the first value that passes the predicate.
         * @param _predicate The predicate to test the value with.
         * @return The first value that passes the predicate. If none passes, nullptr is returned.
         */
        std::shared_ptr<T> find_first_if(std::function<bool(const T&)> _predicate)
        {
            // Get current (head) node.
            node* current = &head_;
            // Lock current mutex.
            writer_lock current_lock(current->mutex_);

            // Get next node
            while (current->next_) {
                // If there is a next node, lock next mutex. Else, end.
                writer_lock next_lock(current->next_->mutex_);

                // If predicate passes, advance next node, and discard the "old" next node.
                if (_predicate(*current->next_->value_)) {
                    return current->next_->value_;
                }

                // If predicate fails, advance current node and lock.
                current = current->next_.get();
                // Advance current lock.
                current_lock = std::move(next_lock);
            }

            return nullptr;
        }

        /**
         * Find and return the first value that passes the predicate.
         * @param _predicate The predicate to test the value with.
         * @return The first value that passes the predicate. If none passes, nullptr is returned.
         */
        std::shared_ptr<const T> find_first_if(std::function<bool(const T&)> _predicate) const
        {
            // Get current (head) node.
            const node* current = &head_;
            // Lock current mutex.
            writer_lock current_lock(current->mutex_);

            // Get next node
            while (current->next_) {
                // If there is a next node, lock next mutex. Else, end.
                writer_lock next_lock(current->next_->mutex_);

                // If predicate passes, advance next node, and discard the "old" next node.
                if (_predicate(*current->next_->value_)) {
                    return std::const_pointer_cast<const T>(current->next_->value_);
                }

                // If predicate fails, advance current node and lock.
                current = current->next_.get();
                // Advance current lock.
                current_lock = std::move(next_lock);
            }

            return nullptr;
        }

        /**
         * Perform the mapper operation on the first value in the list that passes the predicate.
         * @tparam return_type The return type of the mapper function.
         * @param _predicate Predicate to test the values.
         * @param _mapper Mapper to operate on the first value to pass the predicate.
         */
        template<typename return_type>
        std::optional<return_type>
        write_and_map_first_if(std::function<bool(const T&)> _predicate, std::function<return_type(T&)> _mapper)
        {
            // Get current (head) node.
            node* current = &head_;
            // Lock current mutex.
            writer_lock current_lock(head_.mutex_);

            // Get next node
            while (current->next_) {
                // If there is a next node, lock next mutex. Else, end.
                writer_lock next_lock(current->next_->mutex_);

                // If the predicate passes, return the result of applying the mapper on the value wrapped in an optional.
                if (_predicate(*current->next_->value_)) {
                    return std::optional<return_type>{_mapper(*current->next_->value_)};
                }

                // Advance current node.
                current = current->next_.get();
                // Advance current lock.
                current_lock = std::move(next_lock);
            }

            // If none of the values passes the predicate, return an empty optional.
            return std::nullopt;
        }

        /**
         * Perform the mapper operation on the first value in the list that passes the predicate.
         * @tparam return_type The return type of the mapper function.
         * @param _predicate Predicate to test the values.
         * @param _mapper Mapper to operate on the first value to pass the predicate.
         */
        template<typename return_type>
        std::optional<return_type> read_and_map_first_if(std::function<bool(const T&)> _predicate,
                std::function<return_type(const T&)> _mapper) const
        {
            // Get current (head) node.
            const node* current = &head_;
            // Lock current mutex.
            reader_lock current_lock(head_.mutex_);

            // Get next node
            while (current->next_) {
                // If there is a next node, lock next mutex. Else, end.
                reader_lock next_lock(current->next_->mutex_);

                if (_predicate(*current->next_->value_)) {
                    return std::optional<return_type>{_mapper(*current->next_->value_)};
                }

                // Advance current node.
                current = current->next_.get();
                // Advance current lock.
                current_lock = std::move(next_lock);
            }

            // If none of the values passes the predicate, return an empty optional.
            return std::nullopt;
        }

        /*
         * Clear the list.
         */
        void clear()
        {
            // Lock head mutex (prevents pushing).
            writer_lock head_lock(head_.mutex_);
            // Get next node
            while (head_.next_) {
                // If there is a next node, lock next mutex. Else, end.
                writer_lock next_lock(head_.next_->mutex_);
                // Point to the node we want to remove so that it does not go out of scope until we are done.
                std::unique_ptr<node> node_to_remove = std::move(head_.next_);
                // Advance next node.
                head_.next_ = std::move(node_to_remove->next_);
                /* Unlock mutex before it goes out of scope, since there's no guarantee that the
                 * lock gets destructed before the node and it's mutex. (Fuck this bug took me an hour to find.) */
                next_lock.unlock();
                // Decrease element count.
                --num_elements_;
            }
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
        size_t size() const { return num_elements_.load(); }
    };
}

#endif //MKR_MULTITHREAD_LIBRARY_THREADSAFE_LIST_H
