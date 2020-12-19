//
// Created by lnxterry on 20/12/20.
//

#ifndef MKR_MULTITHREAD_LIBRARY_MERGESORT_DEMO_H
#define MKR_MULTITHREAD_LIBRARY_MERGESORT_DEMO_H

#include <iostream>
#include <cstring>
#include "../thread_pool/thread_pool.h"

namespace mkr {
    /**
     * A demo of the performance of mkr::thread_pool using mergesort.
     */
    class mergesort_demo {
    public:
        template<typename T>
        static void do_sort(T* _array, T* _temp_buffer, int _start, int _mid, int _end)
        {
            int left_index = _start;
            int right_index = _mid;

            for (int i = _start; i<_end; i++) {
                if (left_index==_mid) {
                    _temp_buffer[i] = _array[right_index++];
                }
                else if (right_index==_end) {
                    _temp_buffer[i] = _array[left_index++];
                }
                else if (_array[left_index]<_array[right_index]) {
                    _temp_buffer[i] = _array[left_index++];
                }
                else {
                    _temp_buffer[i] = _array[right_index++];
                }
            }

            for (int i = _start; i<_end; i++) {
                _array[i] = _temp_buffer[i];
            }
        }

        template<typename T>
        static void single_thread_mergesort(T* _array, T* _temp_buffer, int _start, int _end)
        {
            int num_elements = _end-_start;
            if (num_elements<=1) { return; }

            int mid = _start+((num_elements) >> 1);
            single_thread_mergesort(_array, _temp_buffer, _start, mid);
            single_thread_mergesort(_array, _temp_buffer, mid, _end);

            do_sort(_array, _temp_buffer, _start, mid, _end);
        }

        template<typename T>
        static void
        thread_pool_mergesort(T* _array, T* _temp_buffer, int _start, int _end, thread_pool* _thread_pool,
                int _granularity)
        {
            int num_elements = _end-_start;
            if (num_elements<=1) { return; }

            int mid = _start+(num_elements >> 1);

            // If num_elements < _granularity, it is not worth the overhead to spawn a new task.
            std::future<void> fork;
            if (num_elements>=_granularity) {
                fork = _thread_pool->submit([_array, _temp_buffer, _start, mid, _thread_pool, _granularity]() {
                    thread_pool_mergesort(_array, _temp_buffer, _start, mid, _thread_pool, _granularity);
                });
            }
            else {
                thread_pool_mergesort(_array, _temp_buffer, _start, mid, _thread_pool, _granularity);
            }

            thread_pool_mergesort(_array, _temp_buffer, mid, _end, _thread_pool, _granularity);

            // If num_elements < _granularity, it is not worth the overhead to spawn a new task.
            if (num_elements>=_granularity) {
                /* If fork isn't ready, we run pending tasks on the thread pool.
                 * If we do not run any pending task, since this function is recursive, there is a high
                 * chance that all the threads may end up simply waiting for fork.get() and no threads
                 * are actually doing any work, resulting in a deadlock.
                 */
                while (!is_future_ready(fork)) {
                    _thread_pool->run_pending_task();
                }
                fork.get();
            }

            do_sort(_array, _temp_buffer, _start, mid, _end);
        }

        template<typename T>
        static void async_mergesort(T* _array, T* _temp_buffer, int _start, int _end, int _granularity)
        {
            int num_elements = _end-_start;
            if (num_elements<=1) { return; }

            int mid = _start+(num_elements >> 1);

            // If num_elements < _granularity, it is not worth the overhead to spawn a new thread.
            std::future<void> fork;
            if (num_elements>=_granularity) {
                fork = std::async([_array, _temp_buffer, _start, mid, _granularity]() -> void {
                    async_mergesort(_array, _temp_buffer, _start, mid, _granularity);
                });
            }
            else {
                async_mergesort(_array, _temp_buffer, _start, mid, _granularity);
            }
            async_mergesort(_array, _temp_buffer, mid, _end, _granularity);

            // If num_elements < _granularity, it is not worth the overhead to spawn a new thread.
            if (num_elements>=_granularity) { fork.get(); }

            do_sort(_array, _temp_buffer, _start, mid, _end);
        }

        template<typename T>
        static void print_array(T _array[], int _size)
        {
            for (int i = 0; i<_size; ++i) {
                std::cout << _array[i] << ", ";
            }
            std::cout << std::endl;
        }

        /**
         * Call this function to run the merge sort demo.
         * @param _num_loops The number of times each test (Single Thread, mkr::thread_pool, std::async) runs. The more times a test run, the more accurate the average time.
         * @param _array_size The number of elements to sort.
         * @param _granularity For mkr::thread_pool and std::async, a new task will be submitted to the thread pool (mkr::thread_pool), or a new thread will only be created (std::async),
         * when the number of elements to sort is greater than this number. There is an inherent cost to creating a new thread or submitting a task.
         * Therefore, if there is only a small number of elements to sort, it may be cheaper to just run it all in the current thread than to split half of it to another thread.
         * @param _display_unsorted Display the unsorted array.
         * @param _display_sorted Display the sorted array.
         * @warning If the number of elements is too large (e.g. 5000000), std::async may not be able to handle it and freeze your system! mkr::thread_pool should work just fine though. :D
         */
        static void
        run(int _num_loops = 4, int _array_size = 500000, int _granularity = 2000, bool _display_unsorted = false,
                bool _display_sorted = false)
        {
            int* unsorted_array = new int[_array_size];
            // Fill the unsorted array with random numbers.
            for (int i = 0; i<_array_size; i++) {
                unsorted_array[i] = (int(std::rand())%_array_size);
            }

            std::cout << "Number of Hardware Threads Your System Supports: " << std::thread::hardware_concurrency() << "\n" << std::endl;

            if (_display_unsorted) {
                std::cout << "Unsorted Array: ";
                print_array(&unsorted_array[0], _array_size);
                std::cout << std::endl;
            }

            // Single Thread
            {
                std::cout << "Merge Sort " << _array_size << " Numbers (Single Thread)" << std::endl;
                long total_duration = 0;
                for (int i = 0; i<_num_loops; ++i) {
                    int* sorted_array = new int[_array_size];
                    int* temp_buffer = new int[_array_size];
                    std::memcpy(&sorted_array[0], &unsorted_array[0], _array_size*sizeof(unsorted_array[0]));

                    auto start_time = std::chrono::high_resolution_clock::now();
                    single_thread_mergesort(&sorted_array[0], &temp_buffer[0], 0, _array_size);
                    auto end_time = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time-start_time).count();

                    total_duration += duration;

                    if (_display_sorted) {
                        std::cout << "Sorted Array: ";
                        print_array(&sorted_array[0], _array_size);
                    }

                    delete[] sorted_array;
                    delete[] temp_buffer;
                }
                std::cout << "Total Time Taken: " << total_duration << "ms" << std::endl;
                std::cout << "Average Time Taken (" << _num_loops << " Loops): " << total_duration/_num_loops << "ms"
                          << std::endl;
                std::cout << std::endl;
            }

            // mkr::thread_pool
            {
                size_t num_threads[] = {2, 6, 12, 16};
                for (int n = 0; n<sizeof(num_threads)/sizeof(num_threads[0]); ++n) {
                    std::cout << "Merge Sort " << _array_size << " Numbers (mkr::thread_pool - " << num_threads[n]
                              << " Threads)" << std::endl;
                    long total_duration = 0;
                    for (int i = 0; i<_num_loops; ++i) {
                        thread_pool tp{num_threads[n]-1};
                        int* sorted_array = new int[_array_size];
                        int* temp_buffer = new int[_array_size];
                        std::memcpy(&sorted_array[0], &unsorted_array[0], _array_size*sizeof(unsorted_array[0]));

                        auto start_time = std::chrono::high_resolution_clock::now();
                        thread_pool_mergesort(&sorted_array[0], &temp_buffer[0], 0, _array_size, &tp, _granularity);
                        auto end_time = std::chrono::high_resolution_clock::now();
                        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                                end_time-start_time).count();
                        total_duration += duration;

                        if (_display_sorted) {
                            std::cout << "Sorted Array: ";
                            print_array(&sorted_array[0], _array_size);
                        }

                        delete[] sorted_array;
                        delete[] temp_buffer;
                    }
                    std::cout << "Total Time Taken: " << total_duration << "ms" << std::endl;
                    std::cout << "Average Time Taken (" << _num_loops << " Loops): " << total_duration/_num_loops
                              << "ms"
                              << std::endl;
                    std::cout << std::endl;
                }
            }

            // std::async
            {
                std::cout << "Merge Sort " << _array_size << " Numbers (std::async)" << std::endl;
                long total_duration = 0;
                for (int i = 0; i<_num_loops; ++i) {
                    int* sorted_array = new int[_array_size];
                    int* temp_buffer = new int[_array_size];
                    std::memcpy(&sorted_array[0], &unsorted_array[0], _array_size*sizeof(unsorted_array[0]));

                    auto start_time = std::chrono::high_resolution_clock::now();
                    async_mergesort(&sorted_array[0], &temp_buffer[0], 0, _array_size, _granularity);
                    auto end_time = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time-start_time).count();

                    total_duration += duration;

                    if (_display_sorted) {
                        std::cout << "Sorted Array: ";
                        print_array(&sorted_array[0], _array_size);
                    }

                    delete[] sorted_array;
                    delete[] temp_buffer;
                }
                std::cout << "Total Time Taken: " << total_duration << "ms" << std::endl;
                std::cout << "Average Time Taken (" << _num_loops << " Loops): " << total_duration/_num_loops << "ms"
                          << std::endl;
                std::cout << std::endl;
            }

            delete[] unsorted_array;
        }
    };
}

#endif //MKR_MULTITHREAD_LIBRARY_MERGESORT_DEMO_H