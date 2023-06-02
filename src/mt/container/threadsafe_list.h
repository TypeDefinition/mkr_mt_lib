#pragma once

#include "container.h"
#include "mt/util/concepts.h"

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <optional>
#include <type_traits>

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
    class threadsafe_list : public container {
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

        /**
         * Internal copy constructor helper function.
         * @param _threadsafe_list The threadsafe_list to copy.
         */
        void do_copy_constructor(const threadsafe_list* _threadsafe_list)
            requires std::copyable<T>
        {
            std::function<void(const T&)> copy_func = [this](const T& _value) {
                this->push_front(_value);
            };
            _threadsafe_list->read_each(copy_func);
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
         * Copy constructor. There is no guarantee that the order of elements is preserved.
         * @param _threadsafe_list The threadsafe_list to copy.
         */
        threadsafe_list(const threadsafe_list& _threadsafe_list)
        {
            do_copy_constructor(&_threadsafe_list);
        }

        /**
         * Move constructor. There is no guarantee that the order of elements is preserved.
         * @param _threadsafe_list The threadsafe_list to copy.
         */
        threadsafe_list(threadsafe_list&& _threadsafe_list)
        {
            do_copy_constructor(&_threadsafe_list);
        }

        /**
         * Destructs the list.
         */
        virtual ~threadsafe_list() { }

        threadsafe_list operator=(const threadsafe_list&) = delete;
        threadsafe_list operator=(threadsafe_list&&) = delete;

        /**
         * Checks if any of the values in the list passes the predicate.
         * @tparam Predicate The typename of the predicate.
         * @param _predicate Predicate to test the values.
         * @return Returns true if any of the values in the list passes the predicate. Else, returns false.
         */
        template<class Predicate>
        bool match_any(Predicate&& _predicate) const
            requires mkr::is_predicate<Predicate, const T&>
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
                if (std::invoke(std::forward<Predicate>(_predicate), *current->next_->value_)) {
                    return true;
                }
                // If predicate fails, advance current node and current lock.
                current = current->next_.get();
                current_lock = std::move(next_lock);
            }

            // Return false if none of the values passes the predicate.
            return false;
        }

        /**
         * Checks if none of the values in the list passes the predicate.
         * @tparam Predicate The typename of the predicate.
         * @param _predicate Predicate to test the values.
         * @return Returns false if any of the values in the list passes the predicate. Else, returns true.
         */
        template<class Predicate>
        bool match_none(Predicate&& _predicate) const
            requires mkr::is_predicate<Predicate, const T&>
        {
            return !match_any(std::forward<Predicate>(_predicate));
        }

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
         * Remove values in the list that passes the predicate.
         * @tparam Predicate The typename of the predicate.
         * @param _predicate Predicate to test if a value should be removed.
         * @param _limit The maximum number of values to remove.
         * @return The number of values removed.
         */
        template<class Predicate>
        size_t remove_if(Predicate&& _predicate, size_t _limit = SIZE_MAX)
            requires mkr::is_predicate<Predicate, const T&>
        {
            // Remove counter.
            size_t num_removed = 0;
            // Get current (head) node.
            node* current = &head_;
            // Lock current (head) mutex.
            writer_lock current_lock(current->mutex_);

            // Get next node
            while (_limit && current->next_) {
                // If there is a next node, lock next mutex. Else, end.
                writer_lock next_lock(current->next_->mutex_);

                // If predicate passes, advance next node, and discard the "old" next node.
                if (std::invoke(std::forward<Predicate>(_predicate), *current->next_->value_)) {
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
                    // Decrease limit counter.
                    --_limit;
                    continue;
                }

                // If predicate fails, advance current node & lock.
                current = current->next_.get();
                current_lock = std::move(next_lock);
            }

            return num_removed;
        }

        /**
         * Replace the value if it passes the predicate.
         * @tparam Predicate The typename of the predicate.
         * @tparam Supplier The typename of the supplier.
         * @param _predicate Predicate to test if a value should be replaced.
         * @param _supplier Supplier to generate the replacement value.
         * @param _limit The maximum number of values to replace.
         * @return The number of values replaced.
         */
        template<class Predicate, class Supplier>
        size_t replace_if(Predicate&& _predicate, Supplier&& _supplier, size_t _limit = SIZE_MAX)
            requires (mkr::is_predicate<Predicate, const T&> && mkr::is_supplier<Supplier, T>)
        {
            size_t num_replaced = 0;
            // Get current (head) node.
            node* current = &head_;
            // Lock current mutex.
            writer_lock current_lock(head_.mutex_);

            // Get next node
            while (_limit && current->next_) {
                // If there is a next node, lock next mutex. Else, end.
                writer_lock next_lock(current->next_->mutex_);

                // If the predicate passes, replace the value.
                if (std::invoke(std::forward<Predicate>(_predicate), *current->next_->value_)) {
                    current->next_->value_ = std::make_shared<T>(std::invoke(std::forward<Supplier>(_supplier)));
                    // Increase the replace counter.
                    num_replaced++;
                    // Decrease limit counter.
                    --_limit;
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
         * write_each allows modifying the value in the threadsafe_list.
         * @tparam Consumer The typename of the consumer.
         * @param _consumer Consumer to operate on the values.
         */
        template<class Consumer>
        void write_each(Consumer&& _consumer)
            requires mkr::is_consumer<Consumer, T&>
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
                std::invoke(std::forward<Consumer>(_consumer), *current->next_->value_);

                // Advance current node.
                current = current->next_.get();
                // Advance current lock.
                current_lock = std::move(next_lock);
            }
        }

        /**
         * Perform the consumer operation on each value in the list.
         * read_each does not allow modifying the value in the threadsafe_list.
         * @tparam Consumer The typename of the consumer.
         * @param _consumer Consumer to operate on the values.
         */
        template<class Consumer>
        void read_each(Consumer&& _consumer) const
            requires mkr::is_consumer<Consumer, const T&>
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
                std::invoke(std::forward<Consumer>(_consumer), *current->next_->value_);

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
        template<class Predicate>
        std::shared_ptr<T> find_first_if(Predicate&& _predicate)
            requires mkr::is_predicate<Predicate, const T&>
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
                if (std::invoke(std::forward<Predicate>(_predicate), *current->next_->value_)) {
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
         * @tparam Predicate The typename of the predicate.
         * @param _predicate The predicate to test the value with.
         * @return The first value that passes the predicate. If none passes, nullptr is returned.
         */
        template<class Predicate>
        std::shared_ptr<const T> find_first_if(Predicate&& _predicate) const
            requires mkr::is_predicate<Predicate, const T&>
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
                if (std::invoke(std::forward<Predicate>(_predicate), *current->next_->value_)) {
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
         * write_and_map_first_if allows modifying the values in the threadsafe_list.
         * @tparam Predicate The typename of the predicate.
         * @tparam Mapper The typename of the mapper function.
         * @param _predicate Predicate to test the values.
         * @param _mapper Mapper to operate on the first value to pass the predicate.
         * @return A std::optional containing the return value of the mapper function if a value passing the predicate was found.
         * Else, returns a std::nullopt.
         */
        template<typename Predicate, typename Mapper>
        std::optional<std::invoke_result_t<Mapper, T&>> write_and_map_first_if(Predicate&& _predicate, Mapper&& _mapper)
            requires (mkr::is_predicate<Predicate, const T&> && mkr::is_function<Mapper, T&>)
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
                if (std::invoke(std::forward<Predicate>(_predicate), *current->next_->value_)) {
                    return std::optional<std::invoke_result_t<Mapper, T&>>{
                            std::invoke(std::forward<Mapper>(_mapper), *current->next_->value_)};
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
         * read_and_map_first_if does not allow modifying the values in the threadsafe_list
         * @tparam Predicate The typename of the predicate.
         * @tparam Mapper The typename of the Mapper.
         * @param _predicate Predicate to test the values.
         * @param _mapper Mapper to operate on the first value to pass the predicate.
         * @return The return value of mapper if a value passes the predicate. Else, returns a std::nullopt.
         */
        template<class Predicate, class Mapper>
        std::optional<std::invoke_result_t<Mapper, const T&>> read_and_map_first_if(Predicate&& _predicate, Mapper&& _mapper) const
            requires (mkr::is_predicate<Predicate, const T&> && mkr::is_function<Mapper, const T&>)
        {
            // Get current (head) node.
            const node* current = &head_;
            // Lock current mutex.
            reader_lock current_lock(head_.mutex_);

            // Get next node
            while (current->next_) {
                // If there is a next node, lock next mutex. Else, end.
                reader_lock next_lock(current->next_->mutex_);

                if (std::invoke(std::forward<Predicate>(_predicate), *current->next_->value_)) {
                    return std::optional<std::invoke_result_t<Mapper, const T&>>{
                            std::invoke(std::forward<Mapper>(_mapper), *current->next_->value_)};
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
         * For every value that passes the predicate, insert the result of applying the mapper on it to another collection.
         * write_and_map_if allows modifying the values in the threadsafe_list.
         * @tparam Predicate The typename of the predicate.
         * @tparam Mapper The typename of the mapper.
         * @tparam Inserter The typename of the inserter.
         * @param _predicate The predicate to test the values.
         * @param _mapper The mapper function.
         * @param _inserter The function to insert the mapper's return value to a collection.
         */
        template<typename Predicate, typename Mapper, typename Inserter>
        void write_and_map_if(Predicate&& _predicate, Mapper&& _mapper, Inserter&& _inserter)
            requires mkr::is_predicate<Predicate, const T&>
                    && mkr::is_function<Mapper, T&>
                    && mkr::is_consumer<Inserter, std::invoke_result_t<Mapper, T&>>
        {
            write_each([&](T& _value) {
                if (std::invoke(std::forward<Predicate>(_predicate), _value)) {
                    std::invoke(std::forward<Inserter>(_inserter), std::invoke(std::forward<Mapper>(_mapper), _value));
                }
            });
        }

        /**
         * For every value that passes the predicate, insert the result of applying the mapper on it to another collection.
         * read_and_map_if does not allow modifying the values in the threadsafe_list
         * @tparam Predicate The typename of the predicate.
         * @tparam Mapper The typename of the mapper.
         * @tparam Inserter The typename of the inserter.
         * @param _predicate The predicate to test the values.
         * @param _mapper The mapper function.
         * @param _inserter The function to insert the mapper's return value to a collection.
         */
        template<typename Predicate, typename Mapper, typename Inserter>
        void read_and_map_if(Predicate&& _predicate, Mapper&& _mapper, Inserter&& _inserter)
            requires mkr::is_predicate<Predicate, const T&>
                    && mkr::is_function<Mapper, const T&>
                    && mkr::is_consumer<Inserter, std::invoke_result_t<Mapper, const T&>>
        {
            read_each([&](const T& _value) {
                if (std::invoke(std::forward<Predicate>(_predicate), _value)) {
                    std::invoke(std::forward<Inserter>(_inserter), std::invoke(std::forward<Mapper>(_mapper), _value));
                }
            });
        }

        /**
         * For every value, insert the result of applying the mapper on it to another collection.
         * write_and_map_each allows modifying the values in the threadsafe_list.
         * @tparam Mapper The typename of the mapper.
         * @tparam Inserter The typename of the inserter.
         * @param _mapper The mapper function.
         * @param _inserter The function to insert the mapper's return value to a collection.
         */
        template<typename Mapper, typename Inserter>
        void write_and_map_each(Mapper&& _mapper, Inserter&& _inserter)
            requires mkr::is_function<Mapper, T&> && mkr::is_consumer<Inserter, std::invoke_result_t<Mapper, T&>>
        {
            write_each([&](T& _value) {
                std::invoke(std::forward<Inserter>(_inserter), std::invoke(std::forward<Mapper>(_mapper), _value));
            });
        }

        /**
         * For every value, insert the result of applying the mapper on it to another collection.
         * read_and_map_each does not allow modifying the values in the threadsafe_list
         * @tparam Mapper The typename of the mapper.
         * @tparam Inserter The typename of the inserter.
         * @param _mapper The mapper function.
         * @param _inserter The function to insert the mapper's return value to a collection.
         */
        template<typename Mapper, typename Inserter>
        void read_and_map_each(Mapper&& _mapper, Inserter&& _inserter)
            requires mkr::is_function<Mapper, const T&> && mkr::is_consumer<Inserter, std::invoke_result_t<Mapper, T&>>
        {
            read_each([&](const T& _value) {
                std::invoke(std::forward<Inserter>(_inserter), std::invoke(std::forward<Mapper>(_mapper), _value));
            });
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