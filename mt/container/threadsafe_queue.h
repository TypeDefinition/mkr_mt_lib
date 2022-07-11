#pragma once

#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include "common/concepts.h"
#include "container.h"

namespace mkr {
    /**
     * Threadsafe queue.
     *
     * Invariants:
     * - tail_->value_ == nullptr
     * - tail->next_ == nullptr
     * - head_ == tail_ implies an empty list.
     * - A single element list has head_->next_ == tail_.
     * - For each node x in the queue, where x!=tail_, x->value_ points to an instance of T and x->next_ points to the next node in the queue.
     * - x->next_==tail implies x is the last node in the queue.
     * - Traversing head_->next_ will eventually lead to tail_.
     *
     * Addtional Requirements:
     * - threadsafe_queue must be able to support type T where T is a non-copyable or non-movable type.
     * - threadsafe_queue does not have to support a non-copyable AND non-movable type.
     *
     * Additional Notes:
     * - threadsafe_queue is non-copyable AND non-movable. (As it has non-copyable and non-movable members.)
     *
     * @tparam T The typename of the contained values.
     */
    template<typename T>
    class threadsafe_queue : public container {
    private:
        typedef std::timed_mutex mutex_type;

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
            /// Value.
            std::shared_ptr<T> value_;
            /// Next node.
            std::unique_ptr<node> next_;

            /**
             * Copy this node and the following nodes.
             * @return A copy this node and the following nodes.
             */
            std::unique_ptr<node> copy() const
            {
                std::unique_ptr<node> copied_node = std::make_unique<node>();
                copied_node->value_ = value_ ? std::make_shared<T>(*value_) : nullptr;
                copied_node->next_ = next_ ? std::move(next_->copy()) : nullptr;
                return copied_node;
            }
        };

        /**
         * Get the tail node.
         * @return The tail node.
         */
        const node* get_tail() const
        {
            std::lock_guard<mutex_type> lock(tail_mutex_);
            return tail_;
        }

        /**
         * Get the tail node.
         * @return The tail node.
         */
        node* get_tail()
        {
            std::lock_guard<mutex_type> lock(tail_mutex_);
            return tail_;
        }

        /**
         * Internal function to push a new value to the tail of the queue.
         * @param _value The value to push to the tail of the queue.
         * @warning If multiple threads pushes at the same time, the value will be inserted but may no longer be at the tail of the queue.
         */
        void do_push(std::shared_ptr<T> _value)
        {
            // Construct a dummy node.
            std::unique_ptr<node> dummy_node = std::make_unique<node>();
            // Lock the tail mutex.
            std::lock_guard<mutex_type> lock(tail_mutex_);
            // Set the tail's value to the new value.
            tail_->value_ = _value;
            // Set the tail's next node to the dummy node.
            tail_->next_ = std::move(dummy_node);
            // Set the dummy node as the tail node.
            tail_ = tail_->next_.get();
            // Increase the element counter.
            num_elements_++;
        }

        /**
         * Internal function to pop a value from the head of the queue.
         * @return Pops and return the value from the head of the queue.
         */
        std::shared_ptr<T> do_pop()
        {
            // Extract the value of the head node.
            std::unique_ptr<node> old_head = std::move(head_);
            // Point the head node to it's next node.
            head_ = std::move(old_head->next_);
            // Decrease the element counter.
            num_elements_--;
            // Return the extracted value.
            return old_head->value_;
        }

        /**
         * Constructs a copy of another threadsafe_queue. The contents of the other threadsafe_queue is copied.
         * @param _threadsafe_queue The threadsafe_queue to copy.
         */
        void do_copy_construct(const threadsafe_queue* _threadsafe_queue) requires std::copyable<T>
        {
            // Lock the mutexes to prevent anyone from pushing.
            std::lock_guard<mutex_type> head_lock(_threadsafe_queue->head_mutex_);
            std::lock_guard<mutex_type> tail_lock(_threadsafe_queue->tail_mutex_);
            // Set the element counter.
            num_elements_ = _threadsafe_queue->num_elements_.load();
            // Copy Nodes
            head_ = _threadsafe_queue->head_ ? std::move(_threadsafe_queue->head_->copy()) : nullptr;
            tail_ = head_.get();
            while (tail_ && tail_->next_) { tail_ = tail_->next_.get(); }
        }

        /// Mutex of the head of the queue.
        mutable mutex_type head_mutex_;
        /// Mutex of the tail of the queue.
        mutable mutex_type tail_mutex_;
        /// Condition variable to notify waiting threads of a push.
        std::condition_variable_any cond_;
        /// Head node of the queue.
        std::unique_ptr<node> head_;
        /// Tail node of the queue.
        node* tail_;
        /// Number of elements in the queue.
        std::atomic_size_t num_elements_;

    public:
        /**
         * Constructs the queue.
         */
        threadsafe_queue()
                :num_elements_(0)
        {
            // Create a dummy node.
            head_ = std::make_unique<node>();
            // When list is empty, head_ == tail_.
            tail_ = head_.get();
        }

        /**
         * Copy constructor.
         * @param _threadsafe_queue The queue to copy from.
         */
        threadsafe_queue(const threadsafe_queue& _threadsafe_queue)
        {
            do_copy_construct(&_threadsafe_queue);
        }

        /**
         * Move constructor.
         * @param _threadsafe_queue The queue to copy from.
         */
        threadsafe_queue(threadsafe_queue&& _threadsafe_queue)
        {
            do_copy_construct(&_threadsafe_queue);
        }

        /**
         * Destructs the stack.
         */
        virtual ~threadsafe_queue() { }

        threadsafe_queue operator=(const threadsafe_queue&) = delete;
        threadsafe_queue operator=(threadsafe_queue&&) = delete;

        /**
         * Push a new value to the tail of the queue.
         * @param _value The value to push to the tail of the queue.
         * @warning If multiple threads pushes at the same time, the value will be inserted but may no longer be at the tail of the queue.
         */
        void push(const T& _value)
        {
            do_push(std::make_shared<T>(_value));
            // Notify any threads waiting for value_. This is done AFTER unlocking the mutex so that any waiting threads can operate immediately.
            cond_.notify_one();
        }

        /**
         * Push a new value to the tail of the queue.
         * @param _value The value to push to the tail of the queue.
         * @warning If multiple threads pushes at the same time, the value will be inserted but may no longer be at the tail of the queue.
         */
        void push(T&& _value)
        {
            do_push(std::make_shared<T>(std::forward<T>(_value)));
            // Notify any threads waiting for value_. This is done AFTER unlocking the mutex so that any waiting threads can operate immediately.
            cond_.notify_one();
        }

        /**
         * Try to remove the front value from the queue.
         * @return Pop and return the value from the front of the queue. If the queue is empty, return a nullptr.
         */
        std::shared_ptr<T> try_pop()
        {
            // Lock the head mutex.
            std::lock_guard<mutex_type> lock(head_mutex_);
            return (head_.get()==get_tail()) ? nullptr : do_pop();
        }

        /**
        * Wait and remove the front value from the queue.
        * @return Pops and return the value from the front of the queue. If the stack is queue, wait until another thread pushes a value.
        */
        std::shared_ptr<T> wait_and_pop()
        {
            // Lock the head mutex.
            std::unique_lock<mutex_type> lock(head_mutex_);
            cond_.wait(lock, [this]() { return head_.get()!=get_tail(); });
            return do_pop();
        }

        /**
         * Clears the queue.
         */
        void clear()
        {
            // Lock the mutexes to prevent anyone from pushing.
            std::lock_guard<mutex_type> head_lock(head_mutex_);
            std::lock_guard<mutex_type> tail_lock(tail_mutex_);
            while (head_.get()!=tail_) { do_pop(); }
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