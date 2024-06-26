#include "mergesort_test.h"
#include <gtest/gtest.h>

using namespace mkr;

TEST(mergesort, correctness) {
    const int array_size = 1000000 * 1;
    const int granularity = 10000;

    int *unsorted_array = new int[array_size];
    // Fill the unsorted array with random numbers.
    for (int i = 0; i < array_size; i++) {
        unsorted_array[i] = (int(std::rand()) % array_size);
    }

    std::cout << "Number of Concurrent Threads Your System Supports: " << std::thread::hardware_concurrency() << "\n" << std::endl;

    // Single Thread
    int *temp_buffer = new int[array_size];
    int *st_sorted_array = new int[array_size];
    int *tp_sorted_array = new int[array_size];
    int* sa_sorted_array = new int[array_size];

    std::memcpy(&st_sorted_array[0], &unsorted_array[0], array_size * sizeof(unsorted_array[0]));
    std::memcpy(&tp_sorted_array[0], &unsorted_array[0], array_size * sizeof(unsorted_array[0]));
    std::memcpy(&sa_sorted_array[0], &unsorted_array[0], array_size * sizeof(unsorted_array[0]));

    {
        std::cout << "Merge Sort " << array_size << " Numbers (Single Thread)" << std::endl;

        auto start_time = std::chrono::high_resolution_clock::now();
        mergesort_test::single_thread_mergesort(&st_sorted_array[0], &temp_buffer[0], 0, array_size);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        std::cout << "Time Taken: " << duration << "ms" << std::endl << std::endl;
    }

    // mkr::thread_pool
    {
        thread_pool tp{};
        std::cout << "Merge Sort " << array_size << " Numbers (mkr::thread_pool - " << tp.num_threads() << " Threads)" << std::endl;

        auto start_time = std::chrono::high_resolution_clock::now();
        mergesort_test::thread_pool_mergesort(&tp_sorted_array[0], &temp_buffer[0], 0, array_size, &tp, granularity);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        std::cout << "Time Taken: " << duration << "ms" << std::endl << std::endl;
    }

    // std::async
    {
        std::cout << "Merge Sort " << array_size << " Numbers (std::async)" << std::endl;

        auto start_time = std::chrono::high_resolution_clock::now();
        mergesort_test::async_mergesort(&sa_sorted_array[0], &temp_buffer[0], 0, array_size, granularity);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        std::cout << "Time Taken: " << duration << "ms" << std::endl << std::endl;
    }

    bool result = true;
    for (auto i = 0; i < array_size; ++i) {
        if (tp_sorted_array[i] != st_sorted_array[i]) {
            result = false;
            break;
        } else if (sa_sorted_array[i] != st_sorted_array[i]) {
            result = false;
            break;
        }
    }

    delete[] temp_buffer;
    delete[] tp_sorted_array;
    delete[] st_sorted_array;
    delete[] sa_sorted_array;

    EXPECT_TRUE(result);
}