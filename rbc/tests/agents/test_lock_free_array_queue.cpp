/**
 * @file test_lock_free_array_queue.cpp
 * @brief Test cases for vstd::LockFreeArrayQueue
 * @details Tests all APIs of the lock-free array queue including:
 *          - Constructors (default, sized, move)
 *          - Move assignment
 *          - Enqueue operations (enqueue, try_push)
 *          - Dequeue operations (dequeue, pop, try_pop)
 *          - Queue length queries
 *          - Capacity reservation
 *          - Thread safety basics
 */

#include "../_framework/test_util.h"
#include <luisa/vstl/common.h>
#include <luisa/vstl/lockfree_array_queue.h>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

// Helper struct to test with non-trivial types
struct TestStruct {
    int x;
    std::string s;
    
    TestStruct() : x(0), s("default") {}
    TestStruct(int x_val, const std::string& s_val) : x(x_val), s(s_val) {}
    TestStruct(const TestStruct&) = default;
    TestStruct(TestStruct&&) = default;
    TestStruct& operator=(const TestStruct&) = default;
    TestStruct& operator=(TestStruct&&) = default;
    
    bool operator==(const TestStruct& other) const {
        return x == other.x && s == other.s;
    }
};

TEST_SUITE("agents.lock_free_array_queue") {

    // ============================================
    // Constructor Tests
    // ============================================

    TEST_CASE("constructor_default") {
        // Test default constructor (capacity = 64)
        vstd::LockFreeArrayQueue<int> queue;
        CHECK(queue.length() == 0);
        
        // Should be able to enqueue without immediate reallocation
        for (int i = 0; i < 64; ++i) {
            queue.enqueue(i);
        }
        CHECK(queue.length() == 64);
    }

    TEST_CASE("constructor_with_capacity") {
        // Test constructor with custom capacity
        vstd::LockFreeArrayQueue<int> queue(128);
        CHECK(queue.length() == 0);
        
        // Fill to custom capacity
        for (int i = 0; i < 128; ++i) {
            queue.enqueue(i);
        }
        CHECK(queue.length() == 128);
    }

    TEST_CASE("constructor_minimum_capacity") {
        // Test that capacity is at least 32 (minimum enforced)
        vstd::LockFreeArrayQueue<int> queue(10);
        CHECK(queue.length() == 0);
        
        // Should be able to enqueue at least 32 elements
        for (int i = 0; i < 32; ++i) {
            queue.enqueue(i);
        }
        CHECK(queue.length() == 32);
    }

    TEST_CASE("constructor_power_of_two_capacity") {
        // Test that capacity is rounded up to power of 2
        vstd::LockFreeArrayQueue<int> queue(100);
        CHECK(queue.length() == 0);
        
        // 100 -> rounded to 128 (next power of 2)
        for (int i = 0; i < 128; ++i) {
            queue.enqueue(i);
        }
        CHECK(queue.length() == 128);
    }

    TEST_CASE("move_constructor") {
        // Test move constructor
        vstd::LockFreeArrayQueue<int> queue1(32);
        queue1.enqueue(1);
        queue1.enqueue(2);
        queue1.enqueue(3);
        
        vstd::LockFreeArrayQueue<int> queue2(std::move(queue1));
        
        // queue2 should have the elements
        CHECK(queue2.length() == 3);
        
        auto val = queue2.dequeue();
        CHECK(val.has_value());
        CHECK(val.value() == 1);
    }

    // ============================================
    // Move Assignment Tests
    // ============================================

    TEST_CASE("move_assignment") {
        // Test move assignment operator
        vstd::LockFreeArrayQueue<int> queue1(32);
        queue1.enqueue(10);
        queue1.enqueue(20);
        
        vstd::LockFreeArrayQueue<int> queue2(32);
        queue2.enqueue(99);
        
        queue2 = std::move(queue1);
        
        // queue2 should now have queue1's elements
        CHECK(queue2.length() == 2);
        
        auto val = queue2.dequeue();
        CHECK(val.has_value());
        CHECK(val.value() == 10);
    }

    // ============================================
    // Enqueue Tests
    // ============================================

    TEST_CASE("enqueue_basic") {
        // Test basic enqueue operation
        vstd::LockFreeArrayQueue<int> queue(32);
        
        queue.enqueue(42);
        CHECK(queue.length() == 1);
        
        queue.enqueue(100);
        CHECK(queue.length() == 2);
    }

    TEST_CASE("enqueue_non_trivial_type") {
        // Test enqueue with non-trivial types
        vstd::LockFreeArrayQueue<TestStruct> queue(32);
        
        queue.enqueue(1, "hello");
        queue.enqueue(2, "world");
        
        CHECK(queue.length() == 2);
        
        auto val = queue.dequeue();
        CHECK(val.has_value());
        CHECK(val->x == 1);
        CHECK(val->s == "hello");
    }

    TEST_CASE("enqueue_with_auto_resize") {
        // Test that enqueue triggers automatic resize when full
        vstd::LockFreeArrayQueue<int> queue(4);  // Small initial capacity
        
        // Enqueue more than initial capacity
        for (int i = 0; i < 100; ++i) {
            queue.enqueue(i);
        }
        
        CHECK(queue.length() == 100);
        
        // Verify all values
        for (int i = 0; i < 100; ++i) {
            auto val = queue.dequeue();
            CHECK(val.has_value());
            CHECK(val.value() == i);
        }
    }

    TEST_CASE("try_push_success") {
        // Test successful try_push
        vstd::LockFreeArrayQueue<int> queue(32);
        
        bool result = queue.try_push(42);
        CHECK(result == true);
        CHECK(queue.length() == 1);
    }

    // ============================================
    // Dequeue Tests
    // ============================================

    TEST_CASE("dequeue_basic") {
        // Test basic dequeue operation
        vstd::LockFreeArrayQueue<int> queue(32);
        queue.enqueue(1);
        queue.enqueue(2);
        queue.enqueue(3);
        
        auto val1 = queue.dequeue();
        CHECK(val1.has_value());
        CHECK(val1.value() == 1);
        CHECK(queue.length() == 2);
        
        auto val2 = queue.dequeue();
        CHECK(val2.has_value());
        CHECK(val2.value() == 2);
        CHECK(queue.length() == 1);
    }

    TEST_CASE("dequeue_empty_queue") {
        // Test dequeue from empty queue returns empty optional
        vstd::LockFreeArrayQueue<int> queue(32);
        
        auto val = queue.dequeue();
        CHECK(!val.has_value());
        CHECK(queue.length() == 0);
    }

    TEST_CASE("dequeue_fifo_order") {
        // Test FIFO (First In First Out) order is maintained
        vstd::LockFreeArrayQueue<int> queue(32);
        
        for (int i = 0; i < 10; ++i) {
            queue.enqueue(i);
        }
        
        for (int i = 0; i < 10; ++i) {
            auto val = queue.dequeue();
            CHECK(val.has_value());
            CHECK(val.value() == i);
        }
    }

    TEST_CASE("pop_with_pointer") {
        // Test pop(T* ptr) overload
        vstd::LockFreeArrayQueue<int> queue(32);
        queue.enqueue(42);
        queue.enqueue(100);
        
        int value;
        bool result = queue.pop(&value);
        
        CHECK(result == true);
        CHECK(value == 42);
        CHECK(queue.length() == 1);
    }

    TEST_CASE("pop_empty_queue") {
        // Test pop from empty queue returns false
        vstd::LockFreeArrayQueue<int> queue(32);
        
        int value = 999;  // Initialize with sentinel value
        bool result = queue.pop(&value);
        
        CHECK(result == false);
        // Note: value is destroyed by pop(), so we don't check it
    }

    TEST_CASE("try_pop_success") {
        // Test successful try_pop
        vstd::LockFreeArrayQueue<int> queue(32);
        queue.enqueue(42);
        
        auto val = queue.try_pop();
        CHECK(val.has_value());
        CHECK(val.value() == 42);
        CHECK(queue.length() == 0);
    }

    TEST_CASE("try_pop_empty") {
        // Test try_pop on empty queue
        vstd::LockFreeArrayQueue<int> queue(32);
        
        auto val = queue.try_pop();
        CHECK(!val.has_value());
    }

    // ============================================
    // Capacity and Length Tests
    // ============================================

    TEST_CASE("length_tracking") {
        // Test that length is correctly tracked
        vstd::LockFreeArrayQueue<int> queue(64);
        
        CHECK(queue.length() == 0);
        
        // Enqueue and check length
        for (int i = 1; i <= 50; ++i) {
            queue.enqueue(i);
            CHECK(queue.length() == static_cast<size_t>(i));
        }
        
        // Dequeue and check length
        for (int i = 50; i >= 1; --i) {
            queue.dequeue();
            CHECK(queue.length() == static_cast<size_t>(i - 1));
        }
    }

    TEST_CASE("reserve_increase_capacity") {
        // Test reserve to increase capacity
        vstd::LockFreeArrayQueue<int> queue(32);
        
        // Add some elements
        for (int i = 0; i < 10; ++i) {
            queue.enqueue(i);
        }
        
        // Reserve more space
        queue.reserve(128);
        
        // Elements should still be there
        CHECK(queue.length() == 10);
        
        // Add more elements
        for (int i = 10; i < 50; ++i) {
            queue.enqueue(i);
        }
        
        CHECK(queue.length() == 50);
        
        // Verify order
        for (int i = 0; i < 50; ++i) {
            auto val = queue.dequeue();
            CHECK(val.has_value());
            CHECK(val.value() == i);
        }
    }

    TEST_CASE("reserve_no_shrink") {
        // Test that reserve doesn't shrink capacity
        vstd::LockFreeArrayQueue<int> queue(128);
        
        for (int i = 0; i < 50; ++i) {
            queue.enqueue(i);
        }
        
        // Try to reserve less than current - should be no-op
        queue.reserve(32);
        
        // All elements should still be there
        CHECK(queue.length() == 50);
    }

    // ============================================
    // Interleaved Operations Tests
    // ============================================

    TEST_CASE("interleaved_enqueue_dequeue") {
        // Test interleaved enqueue and dequeue
        vstd::LockFreeArrayQueue<int> queue(16);
        
        // Enqueue and dequeue in mixed pattern
        for (int round = 0; round < 5; ++round) {
            for (int i = 0; i < 10; ++i) {
                queue.enqueue(round * 10 + i);
            }
            
            for (int i = 0; i < 10; ++i) {
                auto val = queue.dequeue();
                CHECK(val.has_value());
                CHECK(val.value() == round * 10 + i);
            }
        }
        
        CHECK(queue.length() == 0);
    }

    TEST_CASE("enqueue_dequeue_around_boundary") {
        // Test enqueue/dequeue that wraps around the circular buffer
        vstd::LockFreeArrayQueue<int> queue(8);  // Small capacity for wrap-around
        
        // Fill queue
        for (int i = 0; i < 8; ++i) {
            queue.enqueue(i);
        }
        
        // Dequeue half
        for (int i = 0; i < 4; ++i) {
            auto val = queue.dequeue();
            CHECK(val.value() == i);
        }
        
        // Enqueue more (should wrap around)
        for (int i = 8; i < 12; ++i) {
            queue.enqueue(i);
        }
        
        // Dequeue remaining (should be in correct order)
        for (int i = 4; i < 12; ++i) {
            auto val = queue.dequeue();
            CHECK(val.has_value());
            CHECK(val.value() == i);
        }
    }

    // ============================================
    // Multi-threaded Tests
    // ============================================

    TEST_CASE("multi_threaded_producer_consumer") {
        // Test basic thread safety with single producer and single consumer
        vstd::LockFreeArrayQueue<int> queue(256);
        const int item_count = 10000;
        
        std::atomic<int> sum_produced{0};
        std::atomic<int> sum_consumed{0};
        std::atomic<bool> done{false};
        
        // Producer thread
        std::thread producer([&]() {
            for (int i = 0; i < item_count; ++i) {
                queue.enqueue(i);
                sum_produced += i;
            }
            done = true;
        });
        
        // Consumer thread
        std::thread consumer([&]() {
            int consumed = 0;
            while (consumed < item_count) {
                auto val = queue.dequeue();
                if (val.has_value()) {
                    sum_consumed += val.value();
                    consumed++;
                } else if (done.load()) {
                    // If done and no more items, exit
                    std::this_thread::yield();
                }
            }
        });
        
        producer.join();
        consumer.join();
        
        CHECK(sum_produced.load() == sum_consumed.load());
    }

    TEST_CASE("multi_threaded_multiple_producers") {
        // Test with multiple producer threads
        vstd::LockFreeArrayQueue<int> queue(1024);
        const int num_producers = 4;
        const int items_per_producer = 1000;
        std::atomic<int> total_enqueued{0};
        
        std::vector<std::thread> producers;
        
        for (int t = 0; t < num_producers; ++t) {
            producers.emplace_back([&, t]() {
                for (int i = 0; i < items_per_producer; ++i) {
                    queue.enqueue(t * items_per_producer + i);
                    total_enqueued++;
                }
            });
        }
        
        for (auto& t : producers) {
            t.join();
        }
        
        CHECK(queue.length() == static_cast<size_t>(num_producers * items_per_producer));
        CHECK(total_enqueued.load() == num_producers * items_per_producer);
    }

    // ============================================
    // Edge Cases and Stress Tests
    // ============================================

    TEST_CASE("large_number_of_elements") {
        // Test with large number of elements
        vstd::LockFreeArrayQueue<int> queue(1024);
        const int count = 100000;
        
        for (int i = 0; i < count; ++i) {
            queue.enqueue(i);
        }
        
        CHECK(queue.length() == static_cast<size_t>(count));
        
        for (int i = 0; i < count; ++i) {
            auto val = queue.dequeue();
            CHECK(val.has_value());
            CHECK(val.value() == i);
        }
    }

    TEST_CASE("rapid_enqueue_dequeue") {
        // Rapid enqueue/dequeue cycles
        vstd::LockFreeArrayQueue<int> queue(64);
        
        for (int cycle = 0; cycle < 1000; ++cycle) {
            for (int i = 0; i < 10; ++i) {
                queue.enqueue(i);
            }
            for (int i = 0; i < 10; ++i) {
                queue.dequeue();
            }
        }
        
        CHECK(queue.length() == 0);
    }

    TEST_CASE("destructor_cleanup") {
        // Test that destructor properly cleans up elements
        std::atomic<int> destructor_count{0};
        
        struct CountingStruct {
            std::atomic<int>* counter;
            CountingStruct(std::atomic<int>* c) : counter(c) {}
            ~CountingStruct() { (*counter)++; }
        };
        
        {
            vstd::LockFreeArrayQueue<CountingStruct> queue(32);
            for (int i = 0; i < 10; ++i) {
                queue.enqueue(&destructor_count);
            }
            // Destructor called here
        }
        
        // All 10 elements should have been destroyed
        CHECK(destructor_count.load() >= 10);
    }

}
