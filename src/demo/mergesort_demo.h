//
// Created by lnxterry on 20/12/20.
//

#ifndef MKR_MULTITHREAD_LIBRARY_MERGESORT_DEMO_H
#define MKR_MULTITHREAD_LIBRARY_MERGESORT_DEMO_H

#include <iostream>
#include <cstring>
#include "../thread_pool/thread_pool.h"

namespace mkr {
    class mergesort_demo {
    public:
        template<typename T>
        static void single_thread_mergesort(T* _array, size_t _start, size_t _end)
        {
            size_t num_elements = _end-_start;
            if (num_elements<=1) { return; }

            size_t mid = _start+((num_elements) >> 1);
            single_thread_mergesort(_array, _start, mid);
            single_thread_mergesort(_array, mid, _end);

            // Do sorting.
            T* buffer = new T[num_elements];
            size_t left_index = _start;
            size_t right_index = mid;
            for (size_t i = 0; i<num_elements; i++) {
                if (left_index==mid) {
                    buffer[i] = _array[right_index++];
                }
                else if (right_index==_end) {
                    buffer[i] = _array[left_index++];
                }
                else if (_array[left_index]<_array[right_index]) {
                    buffer[i] = _array[left_index++];
                }
                else {
                    buffer[i] = _array[right_index++];
                }
            }
            for (size_t i = 0; i<num_elements; i++) {
                _array[_start+i] = buffer[i];
            }
            delete[] buffer;
        }

        template<typename T>
        static void async_mergesort(T* _array, size_t _start, size_t _end, size_t _granularity)
        {
            size_t num_elements = _end-_start;
            if (num_elements<=1) { return; }

            size_t mid = _start+(num_elements >> 1);
            std::future<void> fork;
            if (num_elements>=_granularity) {
                fork = std::async([_array, _start, mid, _granularity]() -> void {
                    async_mergesort(_array, _start, mid, _granularity);
                });
            }
            else {
                async_mergesort(_array, _start, mid, _granularity);
            }
            async_mergesort(_array, mid, _end, _granularity);

            if (num_elements>=_granularity) { fork.get(); }

            // Do sorting.
            T* buffer = new T[num_elements];
            size_t left_index = _start;
            size_t right_index = mid;
            for (size_t i = 0; i<num_elements; i++) {
                if (left_index==mid) {
                    buffer[i] = _array[right_index++];
                }
                else if (right_index==_end) {
                    buffer[i] = _array[left_index++];
                }
                else if (_array[left_index]<_array[right_index]) {
                    buffer[i] = _array[left_index++];
                }
                else {
                    buffer[i] = _array[right_index++];
                }
            }
            for (size_t i = 0; i<num_elements; i++) {
                _array[_start+i] = buffer[i];
            }
            delete[] buffer;
        }

        template<typename T>
        static void
        thread_pool_mergesort(T* _array, size_t _start, size_t _end, thread_pool* _thread_pool, size_t _granularity)
        {
            size_t num_elements = _end-_start;
            if (num_elements<=1) { return; }

            size_t mid = _start+(num_elements >> 1);
            std::future<void> fork;
            if (num_elements>=_granularity) {
                fork = _thread_pool->submit([_array, _start, mid, _thread_pool, _granularity]() {
                    thread_pool_mergesort(_array, _start, mid, _thread_pool, _granularity);
                });
            }
            else {
                thread_pool_mergesort(_array, _start, mid, _thread_pool, _granularity);
            }
            thread_pool_mergesort(_array, mid, _end, _thread_pool, _granularity);

            if (num_elements>=_granularity) {
                while (!is_future_ready(fork)) {
                    _thread_pool->run_pending_task();
                }
                fork.get();
            }

            // Do sorting.
            T* buffer = new T[num_elements];
            size_t left_index = _start;
            size_t right_index = mid;
            for (size_t i = 0; i<num_elements; i++) {
                if (left_index==mid) {
                    buffer[i] = _array[right_index++];
                }
                else if (right_index==_end) {
                    buffer[i] = _array[left_index++];
                }
                else if (_array[left_index]<_array[right_index]) {
                    buffer[i] = _array[left_index++];
                }
                else {
                    buffer[i] = _array[right_index++];
                }
            }
            for (size_t i = 0; i<num_elements; i++) {
                _array[_start+i] = buffer[i];
            }
            delete[] buffer;
        }

        template<typename T>
        static void print_array(T _array[], size_t _size)
        {
            for (size_t i = 0; i<_size; ++i) {
                std::cout << _array[i] << ", ";
            }
            std::cout << std::endl;
        }

        static void run()
        {
            const size_t num_loops = 4;
            const size_t array_size = 10;
            const size_t granularity = 1;
            const bool display_sort_result = true;
            size_t unsorted_array[array_size];

            /*for (size_t i = 0; i < array_size; i++) {
                unsorted_array[i] = array_size - 1 -i;
            }*/
            // Fill the unsorted array with random numbers.
            for (size_t& i : unsorted_array) {
                i = (size_t(std::rand())%array_size);
            }

            std::cout << "Hardware Threads: " << std::thread::hardware_concurrency() << "\n" << std::endl;

            if (display_sort_result) {
                std::cout << "Unsorted Array: ";
                print_array(&unsorted_array[0], array_size);
                std::cout << std::endl;
            }

            // Single Thread
            {
                std::cout << "Merge Sort " << array_size << " Numbers (Single Thread)" << std::endl;
                long total_duration = 0;
                for (size_t i = 0; i<num_loops; ++i) {
                    size_t sorted_array[array_size];
                    std::memcpy(&sorted_array[0], &unsorted_array[0], sizeof(unsorted_array));

                    auto start_time = std::chrono::high_resolution_clock::now();
                    single_thread_mergesort(&sorted_array[0], 0, array_size);
                    auto end_time = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time-start_time).count();

                    total_duration += duration;

                    if (display_sort_result) {
                        std::cout << "Sorted Array: ";
                        print_array(&sorted_array[0], array_size);
                    }
                }
                std::cout << "Total Time Taken: " << total_duration << "ms" << std::endl;
                std::cout << "Average Time Taken (" << num_loops << " Loops): " << total_duration/num_loops << "ms"
                          << std::endl;
                std::cout << std::endl;
            }

            // std::async
            {
                std::cout << "Merge Sort " << array_size << " Numbers (std::async)" << std::endl;
                long total_duration = 0;
                for (size_t i = 0; i<num_loops; ++i) {
                    size_t sorted_array[array_size];
                    std::memcpy(&sorted_array[0], &unsorted_array[0], sizeof(unsorted_array));

                    auto start_time = std::chrono::high_resolution_clock::now();
                    async_mergesort(&sorted_array[0], 0, array_size, granularity);
                    auto end_time = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time-start_time).count();

                    total_duration += duration;

                    if (display_sort_result) {
                        std::cout << "Sorted Array: ";
                        print_array(&sorted_array[0], array_size);
                    }
                }
                std::cout << "Total Time Taken: " << total_duration << "ms" << std::endl;
                std::cout << "Average Time Taken (" << num_loops << " Loops): " << total_duration/num_loops << "ms"
                          << std::endl;
                std::cout << std::endl;
            }

            // mkr::thread_pool
            {
                size_t num_threads[] = {2, 6, 12, 18, 24};
                for (size_t n = 0; n<sizeof(num_threads)/sizeof(num_threads[0]); ++n) {
                    std::cout << "Merge Sort " << array_size << " Numbers (mkr::thread_pool - " << num_threads[n]
                              << " Threads)" << std::endl;
                    long total_duration = 0;
                    for (size_t i = 0; i<num_loops; ++i) {
                        thread_pool tp{num_threads[n]-1};
                        size_t sorted_array[array_size];
                        std::memcpy(&sorted_array[0], &unsorted_array[0], sizeof(unsorted_array));

                        auto start_time = std::chrono::high_resolution_clock::now();
                        thread_pool_mergesort(&sorted_array[0], 0, array_size, &tp, granularity);
                        auto end_time = std::chrono::high_resolution_clock::now();
                        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                                end_time-start_time).count();
                        total_duration += duration;

                        if (display_sort_result) {
                            std::cout << "Sorted Array: ";
                            print_array(&sorted_array[0], array_size);
                        }
                    }
                    std::cout << "Total Time Taken: " << total_duration << "ms" << std::endl;
                    std::cout << "Average Time Taken (" << num_loops << " Loops): " << total_duration/num_loops << "ms"
                              << std::endl;
                    std::cout << std::endl;
                }
            }
        }
    };
}

#endif //MKR_MULTITHREAD_LIBRARY_MERGESORT_DEMO_H