//
// Created by lnxterry on 24/1/21.
//

#ifndef MKR_MULTITHREAD_LIBRARY_HANDLE_SET_H
#define MKR_MULTITHREAD_LIBRARY_HANDLE_SET_H

#include <vector>
#include <mutex>

namespace mkr {
    typedef uint64_t handle_t;

    class handle_set {
    private:
        static constexpr size_t handle_bytes = 8;
        static constexpr size_t index_bytes = 4;
        static constexpr size_t version_bytes = handle_bytes-index_bytes;

        static constexpr size_t handle_bits = handle_bytes*8;
        static constexpr size_t index_bits = index_bytes*8;
        static constexpr size_t version_bits = version_bytes*8;

        static constexpr handle_t index_bitmask = (handle_t{1} << index_bits)-1;
        static constexpr handle_t version_bitmask = ~index_bitmask;

        /**
         * Creates a 64-bit handle based on the provided version and index.
         * @param _version The number number of the handle.
         * @param _index The index of the handle.
         * @return The created handle.
         */
        static handle_t create_handle(handle_t _version, handle_t _index)
        {
            return (_version << index_bits) | (_index & index_bitmask);
        }

        /**
         * Internal function to check if a handle is valid. The point of this function
         * is so that internal functions can call do_is_valid_handle, which does NOT lock
         * the mutex. This is because the internal functions that call this would have already locked
         * the mutex. If this function locks the mutex again on the same thread, it becomes undefined behaviour.
         * @param _index The index of the handle.
         * @param _handle The handle to check for validity.
         * @return True if the handle is valid, else false.
         * @warning The mutex must be locked prior to calling this function.
         */
        bool do_is_valid_handle(handle_t _index, handle_t _handle)
        {
            return _index<handle_array.size() && handle_array[_index]==_handle;
        }

        /// Mutex
        std::mutex mutex_;
        /// Array of all handles.
        std::vector<handle_t> handle_array;
        /// The number of handles that can be recycled.
        size_t recycle_counter_ = 0;
        /// The index of the next handle to recycle.
        handle_t next_index_ = 0;

    public:
        /**
         * Constructs the handle_set.
         */
        handle_set() { }

        /**
         * Destructs the handle_set.
         */
        ~handle_set() { }

        /**
         * Get the version of the handle.
         * @param _handle The handle to get the version of.
         * @return The version of the handle.
         */
        static handle_t get_version(handle_t _handle) { return _handle >> index_bits; }

        /**
         * Get the index of the handle.
         * @param _handle The handle to get the index of.
         * @return The index of the handle.
         */
        static handle_t get_index(handle_t _handle) { return _handle & index_bitmask; }

        /**
         * Check if a handle is valid.
         * @param _handle The handle to check for validity.
         * @return True if the handle is valid, else false.
         */
        bool is_valid_handle(handle_t _handle)
        {
            handle_t index = get_index(_handle);
            std::lock_guard<std::mutex> lock{mutex_};
            return do_is_valid_handle(index, _handle);
        }

        /**
         * Generate a new handle.
         * @return A new handle.
         */
        handle_t generate_handle()
        {
            std::lock_guard<std::mutex> lock{mutex_};

            // If there are no indices that can be recycles, create a new handle and append it to handle_array.
            if (recycle_counter_==0) {
                handle_t new_handle = create_handle(0, (handle_t) handle_array.size());
                handle_array.push_back(new_handle);
                return new_handle;
            }

            // We need the index and version of the handle we are recycling.
            handle_t index = next_index_;
            handle_t version = get_version(handle_array[index]);

            // next_index_ will point to the index that the discard handle was previously pointing to.
            next_index_ = get_index(handle_array[next_index_]);
            // Decrease recycle_counter_.
            --recycle_counter_;

            // Increase the version number of the recycled handle and store the new handle into handle_array.
            handle_array[index] = create_handle(version+1, index);
            return handle_array[index];
        }

        /**
         * Discards a handle.
         * @param _handle The handle to discard.
         */
        void discard_handle(handle_t _handle)
        {
            // Find out the index and version of the handle we are discard.
            handle_t index = get_index(_handle);
            handle_t version = get_version(_handle);

            std::lock_guard<std::mutex> lock{mutex_};
            if (do_is_valid_handle(index, _handle)) {
                // Use the index portion of this discarded handle to point to the index of the previous discarded handle.
                handle_array[index] = create_handle(version, next_index_);
                // next_index_ now points the index of this discarded handle.
                next_index_ = index;
                // Increase the recycle counter.
                ++recycle_counter_;
            }
        }
    };
}

#endif //MKR_MULTITHREAD_LIBRARY_HANDLE_SET_H