#pragma once

#include <rbc_core/memory.h>
#include <rbc_core/containers/detail/concurrent_queue.h>
#include <cstddef>

namespace rbc {

/**
 * @brief RBC-specific traits for ConcurrentQueue
 * 
 * This traits class replaces the default moodycamel traits with RBC's memory allocation functions.
 * All memory allocations go through RBC's memory management system.
 */
struct RBCConcurrentQueueTraits {
    // General-purpose size type
    typedef std::size_t size_t;

    // Index type for enqueue/dequeue indices
    typedef std::size_t index_t;

    // Block size - smallest controllable unit for enqueue/dequeue
    // Must be a power of 2
    static const size_t BLOCK_SIZE = 32;

    // Threshold for switching to atomic counter-based empty check
    // For block sizes strictly larger than this threshold
    static const size_t EXPLICIT_BLOCK_EMPTY_COUNTER_THRESHOLD = 32;

    // Expected number of full blocks for explicit producers
    // Must be a power of 2
    static const size_t EXPLICIT_INITIAL_INDEX_SIZE = 32;

    // Expected number of full blocks for implicit producers
    // Must be a power of 2
    static const size_t IMPLICIT_INITIAL_INDEX_SIZE = 32;

    // Initial size of hash table mapping thread IDs to implicit producers
    // Must be a power of two, and either 0 or at least 1
    // If 0, implicit production is disabled
    static const size_t INITIAL_IMPLICIT_PRODUCER_HASH_SIZE = 32;

    // Number of items an explicit consumer must consume before rotation
    static const std::uint32_t EXPLICIT_CONSUMER_CONSUMPTION_QUOTA_BEFORE_ROTATE = 256;

    // Maximum number of elements that can be enqueued to a sub-queue
    static const size_t MAX_SUBQUEUE_SIZE = moodycamel::details::const_numeric_max<size_t>::value;

    // Number of times to spin before sleeping when waiting on a semaphore
    // Only affects BlockingConcurrentQueue
    static const int MAX_SEMA_SPINS = 10000;

    // Whether to recycle dynamically-allocated blocks into internal free list
    static const bool RECYCLE_ALLOCATED_BLOCKS = false;

    // Memory allocation functions using RBC's memory management
    static inline void *malloc(size_t size) {
        return rbc_malloc(size);
    }

    static inline void free(void *ptr) {
        rbc_free(ptr);
    }
};

/**
 * @brief RBC ConcurrentQueue - Multi-producer, multi-consumer lock-free queue
 * 
 * This is a type alias for moodycamel::ConcurrentQueue with RBC-specific traits.
 * It uses RBC's memory allocation functions for all memory operations.
 * 
 * @tparam T The type of elements stored in the queue
 * @tparam Traits Traits class (defaults to RBCConcurrentQueueTraits)
 * 
 * @example
 * ```cpp
 * rbc::ConcurrentQueue<int> queue;
 * 
 * // Enqueue items
 * queue.enqueue(42);
 * queue.enqueue(100);
 * 
 * // Dequeue items
 * int value;
 * if (queue.try_dequeue(value)) {
 *     // Process value
 * }
 * 
 * // Using producer/consumer tokens for better performance
 * rbc::ConcurrentQueue<int>::producer_token_t producer(queue);
 * rbc::ConcurrentQueue<int>::consumer_token_t consumer(queue);
 * 
 * queue.enqueue(producer, 200);
 * queue.try_dequeue(consumer, value);
 * ```
 */
template<typename T, typename Traits = RBCConcurrentQueueTraits>
using ConcurrentQueue = moodycamel::ConcurrentQueue<T, Traits>;

// Note: BlockingConcurrentQueue is not currently available in the underlying implementation

// Re-export ProducerToken and ConsumerToken for convenience
using ProducerToken = moodycamel::ProducerToken;
using ConsumerToken = moodycamel::ConsumerToken;

}// namespace rbc
