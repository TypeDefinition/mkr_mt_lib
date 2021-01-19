//
// Created by lnxterry on 8/12/20.
//

#ifndef MKR_MULTITHREAD_LIBRARY_THREADSAFE_STACK_H
#define MKR_MULTITHREAD_LIBRARY_THREADSAFE_STACK_H

#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>

namespace mkr {
    /**
     * Threadsafe stack.
     *
     * Invariants:
     * - top_ == nullptr means that the stack is empty.
     * - Traversing top_->next_ will eventually lead to the bottom node.
     *
     * Addtional Requirements:
     * - threadsafe_stack must be able to support type T where T is a non-copyable or non-movable type.
     * - threadsafe_stack does not have to support a non-copyable AND non-movable type.
     *
     * Additional Notes:
     * - threadsafe_stack is non-copyable AND non-movable. (As it has non-copyable and non-movable members.)
     *
     * @tparam T The typename of the contained values.
     */
    template<typename T>
    class threadsafe_stack {
    private:
        typedef std::timed_mutex mutex_type;

        /**
         * A node containing a value, and the next node in the stack.
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
                copied_node->value_ = std::make_shared<T>(*value_);
                copied_node->next_ = next_ ? std::move(next_->copy()) : nullptr;
                return copied_node;
            }
        };

        /**
         * Internal function to push a new value onto the top of the stack.
         * @param _value The value to push onto the top of the stack.
         * @warning If multiple threads pushes at the same time, the value will be inserted but may no longer be at the top of the stack.
         */
        void do_push(std::shared_ptr<T> _value)
        {
            // Construct a new node.
            std::unique_ptr<node> new_node = std::make_unique<node>();
            // Set the new node's value to the new value.
            new_node->value_ = _value;
            // Lock the top mutex.
            std::lock_guard<mutex_type> lock(top_mutex_);
            // Set the new node's next node to the old top node.
            new_node->next_ = std::move(top_);
            // Set the new node as the top node.
            top_ = std::move(new_node);
            // Increase the element counter.
            ++num_elements_;
        }

        /**
         * Internal function to pop a value from the top of the stack.
         * @return Pops and return the value from the top of the stack.
         */
        std::shared_ptr<T> do_pop()
        {
            // Extract the value of the top node.
            std::shared_ptr<T> head_data = top_->value_;
            // Point the top node to it's next node.
            top_ = std::move(top_->next_);
            // Decrease the element counter.
            --num_elements_;
            // Return the extracted value.
            return head_data;
        }

        void do_copy_construct(const threadsafe_stack* _threadsafe_stack) requires std::is_copy_constructible_v<T>
        {
            // Lock the top mutex.
            std::lock_guard<mutex_type> lock(_threadsafe_stack->top_mutex_);
            // Set the element counter.
            num_elements_ = _threadsafe_stack->num_elements_.load();
            // Copy Nodes
            top_ = _threadsafe_stack->top_ ? std::move(_threadsafe_stack->top_->copy()) : nullptr;
        }

        /// Mutex of the top of the stack.
        mutable mutex_type top_mutex_;
        /// Condition variable to notify waiting threads of a push.
        std::condition_variable_any cond_;
        /// Top node of the stack.
        std::unique_ptr<node> top_;
        /// Number of elements in the stack.
        std::atomic_size_t num_elements_;

    public:
        /**
         * Constructs the stack.
         */
        threadsafe_stack()
                :num_elements_(0) { }

        /**
         * Copy constructor.
         * @param _threadsafe_stack The threadsafe_stack to copy.
         */
        threadsafe_stack(const threadsafe_stack& _threadsafe_stack)
        {
            do_copy_construct(&_threadsafe_stack);
        }

        /**
         * Move constructor.
         * @param _threadsafe_stack The threadsafe_stack to copy.
         */
        threadsafe_stack(threadsafe_stack&& _threadsafe_stack)
        {
            do_copy_construct(&_threadsafe_stack);
        }

        /**
         * Destructs the stack.
         */
        ~threadsafe_stack() { }

        threadsafe_stack operator=(const threadsafe_stack&) = delete;
        threadsafe_stack operator=(threadsafe_stack&&) = delete;

        /**
         * Push a new value to the top of the stack.
         * @param _value The value to push to the top of the stack.
         * @warning If multiple threads pushes at the same time, the value will be inserted but may no longer be at the top of the stack.
         */
        void push(const T& _value)
        {
            // When possible, expensive operations like constructing an object should be done before acquiring the mutex.
            do_push(std::make_shared<T>(_value));
            // Notify any threads waiting for value_. This is done AFTER unlocking the mutex so that any waiting threads can operate immediately.
            cond_.notify_one();
        }

        /**
         * Push a new value to the top of the stack.
         * @param _value The value to push to the top of the stack.
         * @warning If multiple threads pushes at the same time, the value will be inserted but may no longer be at the top of the stack.
         */
        void push(T&& _value)
        {
            // When possible, expensive operations like constructing an object should be done before acquiring the mutex.
            do_push(std::make_shared<T>(std::forward<T>(_value)));
            // Notify any threads waiting for value_. This is done AFTER unlocking the mutex so that any waiting threads can operate immediately.
            cond_.notify_one();
        }

        /**
        * Wait and remove the top value from the stack.
        * @return Pops and return the value from the top of the stack. If the stack is empty, wait until another thread pushes a value.
        */
        std::shared_ptr<T> wait_and_pop()
        {
            // Lock the top mutex.
            std::unique_lock<mutex_type> lock(top_mutex_);
            cond_.wait(lock, [this]() { return top_!=nullptr; });
            return do_pop();
        }

        /**
         * Try to remove the top value from the stack.
         * @return Pop and return the value from the top of the stack. If the stack is empty, return a nullptr.
         */
        std::shared_ptr<T> try_pop()
        {
            // Lock the top mutex.
            std::lock_guard<mutex_type> lock(top_mutex_);
            if (top_==nullptr) { return nullptr; }
            return do_pop();
        }

        /**
         * Clears the stack.
         */
        void clear()
        {
            // Lock the mutex and prevent anyone from pushing.
            std::lock_guard<mutex_type> lock(top_mutex_);
            // Pop all nodes.
            while (top_!=nullptr) { do_pop(); }
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

#endif //MKR_MULTITHREAD_LIBRARY_THREADSAFE_STACK_H