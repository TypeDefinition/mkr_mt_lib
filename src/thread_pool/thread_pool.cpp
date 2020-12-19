//
// Created by lnxterry on 16/12/20.
//

#include "thread_pool.h"

namespace mkr {
    thread_pool::thread_pool(size_t _num_threads)
            :num_threads_{std::max<size_t>(_num_threads, 1)}, end_flag_{false}, start_flag_{1}
    {
        try {
            // Create the worker threads.
            for (size_t i = 0; i<num_threads_; ++i) {
                local_task_queues_.push_back(std::make_shared<threadsafe_stack<task>>());
                std::thread worker_thread{&thread_pool::worker_thread_func, this};
                worker_index_lookup_.insert(worker_thread.get_id(), i);
                worker_threads_.emplace_back(std::move(worker_thread));
            }

            // Create the local task queues BEFORE starting the threads so that the threads do not try to pop a non-existent queue.
            std::atomic_thread_fence(std::memory_order::release);
            // TODO: Change to std::latch once Ubuntu releases G++ 10
            --start_flag_;
            // start_flag_.count_down();
        }
        catch (...) {
            // Set end_flag_ = true before decreasing the counter on latch, so that the worker threads do not enter the while loop.
            end_flag_ = true;
            // TODO: Change to std::latch once Ubuntu releases G++ 10
            --start_flag_;
            // start_flag_.count_down();
            throw;
        }
    }

    thread_pool::~thread_pool()
    {
        end_flag_ = true;
        for (std::thread& worker_thread : worker_threads_) {
            worker_thread.join();
        }
    }

    std::shared_ptr<task> thread_pool::get_local_task(size_t _index)
    {
        return local_task_queues_[_index]->try_pop();
    }

    std::shared_ptr<task> thread_pool::get_global_task()
    {
        return global_task_queue_.try_pop();
    }

    std::shared_ptr<task> thread_pool::steal_task(size_t _index)
    {
        for (size_t i = 1; i<num_threads_; ++i) {
            std::shared_ptr<task> stolen_task = local_task_queues_[(_index+i)%num_threads_]->try_pop();
            if (stolen_task) {
                return stolen_task;
            }
        }
        return nullptr;
    }

    bool thread_pool::run_local_task(size_t _index)
    {
        std::shared_ptr<task> local_task = get_local_task(_index);
        if (local_task) {
            local_task->operator()();
            return true;
        }
        return false;
    }

    bool thread_pool::run_global_task()
    {
        std::shared_ptr<task> global_task = get_global_task();
        if (global_task) {
            global_task->operator()();
            return true;
        }
        return false;
    }

    bool thread_pool::run_stolen_task(size_t _index)
    {
        std::shared_ptr<task> stolen_task = steal_task(_index);
        if (stolen_task) {
            stolen_task->operator()();
            return true;
        }
        return false;
    }

    void thread_pool::worker_thread_func()
    {
        // TODO: Change to std::latch once Ubuntu releases G++ 10
        while (start_flag_!=0);
        // start_flag_.wait();

        std::size_t worker_index = *worker_index_lookup_.at(std::this_thread::get_id());
        while (!end_flag_.load()) {
            if (!run_local_task(worker_index) &&
                    !run_global_task() &&
                    !run_stolen_task(worker_index)) {
                // If no task was run, yield so that another thread which may have work to do may have priority.
                std::this_thread::yield();
            }
        }
    }

    bool thread_pool::run_pending_task()
    {
        std::shared_ptr<size_t> worker_index_ptr = worker_index_lookup_.at(std::this_thread::get_id());
        if (worker_index_ptr) {
            return run_local_task(*worker_index_ptr) || run_global_task() || run_stolen_task(*worker_index_ptr);
        }

        return run_global_task() || run_stolen_task(0);
    }
}
