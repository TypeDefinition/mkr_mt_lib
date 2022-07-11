#pragma once

#include <functional>
#include <memory>

namespace mkr {
    /**
      * A task is used when there is a need to point to callable objects of different types. It is able to store the callable objects using type erasure.
      * One such usage is in mkr::thread_pool, where a queue of task is used to point to the various functors submitted to the thread pool.
      */
    class task {
    private:
        class wrapper_interface {
        public:
            wrapper_interface() = default;
            virtual ~wrapper_interface() = default;
            virtual void operator()() = 0;
        };

        template<typename Callable>
        class templated_wrapper : public wrapper_interface {
        private:
            Callable func_;
        public:
            explicit templated_wrapper(Callable&& _func)
                    :func_(std::forward<Callable>(_func)) { }
            virtual ~templated_wrapper() = default;
            virtual void operator()() { std::invoke(func_); }

            // Callable might either be non-movable or non-copyable. So template_wrapper supports neither.
            // Once created, a templated_wrapper should not be moved or copied.
            templated_wrapper(templated_wrapper&&) = delete;
            templated_wrapper& operator=(templated_wrapper&&) = delete;
            templated_wrapper(const templated_wrapper&) = delete;
            templated_wrapper& operator=(const templated_wrapper&) = delete;
        };

        /// The wrapped callable. We cannot move or copy wrapper_interface, but we can move the pointer to wrapper interface.
        std::unique_ptr<wrapper_interface> function_ptr_;

    public:
        /**
         * Constructs the task.
         * @tparam Callable The callable to invoke.
         * @param _func The callable object.
         */
        template<typename Callable>
        explicit task(Callable&& _func)
        {
            // By using std::forward, the callable will be constructed via move constructor or copy constructor accordingly.
            function_ptr_ = std::make_unique<templated_wrapper<Callable>>(std::forward<Callable>(_func));
        }

        /**
         * Move constructor.
         * @param _task The other task to move from.
         * @attention This constructor must be specified, otherwise calling the move constructor will result in the templated constructor being constructed.
         *            That will result in a task that creates and calls a task, which creates and calls a task, which creates and calls a task, which creates and calls...
         */
        task(task&& _task) noexcept
        {
            function_ptr_ = std::move(_task.function_ptr_);
        }

        /**
         * Destructs the task.
         */
        ~task() = default;

        /**
         * Move assignment operator.
         * @param _task The other task to move from.
         * @return This task after the move.
         */
        task& operator=(task&& _task) noexcept
        {
            function_ptr_ = std::move(_task.function_ptr_);
            return *this;
        }

        /**
         * Invoke the wrapped callable object.
         */
        void operator()() { function_ptr_->operator()(); }

        task(const task&) = delete;
        task& operator=(const task&) = delete;
    };
}