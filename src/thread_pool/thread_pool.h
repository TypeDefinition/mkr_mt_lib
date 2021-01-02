//
// Created by lnxterry on 16/12/20.
//

#ifndef MKR_MULTITHREAD_LIBRARY_THREAD_POOL_H
#define MKR_MULTITHREAD_LIBRARY_THREAD_POOL_H

#include <thread>
#include <future>
// #include <latch>

#include "../container/threadsafe_hashtable.h"
#include "../container/threadsafe_queue.h"
#include "../container/threadsafe_stack.h"
#include "task.h"

namespace mkr {
    /**
     * Checks if a std::future is ready.
     * @tparam T The data type of the std::future.
     * @param _future The std::future to check.
     * @return Returns true if the std::future is ready. Else, returns false.
     * @warning The behavior is undefined if _future.valid()==false before the call to this function.
     */
    template<typename T>
    bool is_future_ready(const std::future<T>& _future)
    {
        return _future.wait_for(std::chrono::milliseconds(0))==std::future_status::ready;
    }

    /**
     * A work stealing thread pool. Tasks can be submitted to it to be done concurrently.
     * Once a thread is working on a task, it is not interruptable until the task is complete.
     */
    class thread_pool {
    private:
        /// The number of threads in the thread pool. The number of threads must be >= 1.
        const size_t num_threads_;
        /// A flag to signal the threads to start working on tasks.
        std::atomic_int start_flag_; // TODO: Change to std::latch once Ubuntu releases G++ 10
        // std::latch start_flag_;
        /// A flag to signal the threads to stop after completing their current task.
        std::atomic_bool end_flag_;

        /// A hashtable mapping the std::thread::id of a thread to the local array index.
        threadsafe_hashtable<std::thread::id, size_t> worker_index_lookup_;
        /// The global task queue shared by all threads. It is a FIFO queue.
        threadsafe_queue<task> global_task_queue_;
        /**
         * An array of local task queues of the worker threads. It is a LIFO stack.
         * It is best that each worker thread adds the task to it's own stack.
         * A stack container is used instead of a queue container, because when a worker
         * just submits a task, there is a higher chance that the memory that the task will
         * access is still warm in the thread's cache. Therefore there is the least
         * chance of a cache miss. That's why a stack which is LIFO is preferred.
         */
        std::vector<std::shared_ptr<threadsafe_stack<task>>> local_task_queues_;
        /// An array of worker threads.
        std::vector<std::thread> worker_threads_;

        /**
         * Retrieve a task from a worker thread's local task queue.
         * @param _index The thread's array index.
         * @return A task from a worker thread's local task queue. If there are not tasks available, return a nullptr.
         */
        std::shared_ptr<task> get_local_task(size_t _index);

        /**
         * Retrieve a task from the global task queue.
         * @param _index The thread's array index.
         * @return A task from the global task queue. If there are not tasks available, return a nullptr.
         */
        std::shared_ptr<task> get_global_task();

        /**
         * Steal a task from another thread's local task queue.
         * @param _index The stealing thread's array index.
         * @return A task from the global task queue. If there are not tasks available, return a nullptr.
         */
        std::shared_ptr<task> steal_task(size_t _index);

        /**
         * Run a local task.
         * @param _index The thread's array index.
         * @return Returns true if a task was run. Else, return false.
         */
        bool run_local_task(size_t _index);

        /**
         * Run a global task.
         * @return Returns true if a task was run. Else, return false.
         */
        bool run_global_task();

        /**
         * Run a stolen task.
         * @param _index The stealing thread's array index.
         * @return Returns true if a task was run. Else, return false.
         */
        bool run_stolen_task(size_t _index);

        /**
         * Worker thread function.
         */
        void worker_thread_func();

    public:
        /**
         * Constructs the thread pool.
         * @param _num_threads The number of worker threads the thread pool has. Must be 1 or greater.
         */
        thread_pool(size_t _num_threads = static_cast<size_t>(std::thread::hardware_concurrency()-1));
        /**
         * Destructs the thread pool.
         */
        ~thread_pool();

        inline size_t num_threads() const { return num_threads_; }

        /**
         * Run a pending task in the thread pool. After submitting a task to the thread pool, make sure
         * run pending tasks in a while loop if the thread is idle and waiting for the submitted task,
         * especially in recursive functions. In a recursive function, it is very possible that all of the
         * threads are waiting for a submitted task, but none of them is completing any because their current
         * task is to wait for another task.
         * @return Returns true if a task was run. Else, return false.
         * Just because run_pending_task() returns false now, does not mean that another task won't
         * submitted by another thread soon.
         */
        bool run_pending_task();

        /**
         * While a std::future is not ready, run pending tasks.
         * @tparam T The std::future type.
         * @param _future The std::future to check if it is ready.
         * @warning The behavior is undefined if _future.valid()==false before the call to this function.
         */
        template<typename T>
        void run_pending_tasks(const std::future<T>& _future)
        {
            while (!is_future_ready(_future)) {
                run_pending_task();
            }
        }

        /**
         * Submit a task to the thread pool.
         * @tparam Callable The typename of the function or callable object.
         * @tparam Args The typename of the function arguments.
         * @param _func The function or callable object.
         * @param _args The function arguments.
         * @return A std::future which will contain the result of the function.
         */
        template<typename Callable, typename... Args>
        std::future<std::invoke_result_t<Callable, Args...>> submit(Callable&& _func, Args&& ... _args)
        {
            typedef std::invoke_result_t<Callable, Args...> result_t;

            // C++ 20 perfect capture. [https://stackoverflow.com/questions/47496358/c-lambdas-how-to-capture-variadic-parameter-pack-from-the-upper-scope]
            std::packaged_task<result_t(void)> p_task{
                    [func = std::forward<Callable>(_func),
                            ...args = std::forward<Args>(_args)]() {
                        return std::invoke(func, args...);
                    }};
            std::future<result_t> result = p_task.get_future();

            // Get the worker index of this thread.
            // If the task was submitted from a non worker thread, it will not have a worker index.
            // In that case, add the task to the global queue.
            std::shared_ptr<size_t> worker_index = worker_index_lookup_.at(std::this_thread::get_id());
            if (worker_index) {
                local_task_queues_[*worker_index]->push(task{std::move(p_task)});
            }
            else {
                global_task_queue_.push(task{std::move(p_task)});
            }

            return result;
        }
    };
}

#endif //MKR_MULTITHREAD_LIBRARY_THREAD_POOL_H