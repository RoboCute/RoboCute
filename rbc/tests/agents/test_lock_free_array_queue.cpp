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


    // ============================================
    // Constructor Tests
    // ============================================

    "constructor_default"_test = [] {
        // Test default constructor (capacity = 64)
        vstd::LockFreeArrayQueue<int> queue;
        expect(static_cast<bool>(queue.length() == 0));
        
        // Should be able to enqueue without immediate reallocation
        for (int i = 0; i < 64; ++i) {
            queue.enqueue(i);
        }
        expect(static_cast<bool>(queue.length() == 64));
    };

    "constructor_with_capacity"_test = [] {
        // Test constructor with custom capacity
        vstd::LockFreeArrayQueue<int> queue(128);
        expect(static_cast<bool>(queue.length() == 0));
        
        // Fill to custom capacity
        for (int i = 0; i < 128; ++i) {
            queue.enqueue(i);
        }
        expect(static_cast<bool>(queue.length() == 128));
    };

    "constructor_minimum_capacity"_test = [] {
        // Test that capacity is at least 32 (minimum enforced)
        vstd::LockFreeArrayQueue<int> queue(10);
        expect(static_cast<bool>(queue.length() == 0));
        
        // Should be able to enqueue at least 32 elements
        for (int i = 0; i < 32; ++i) {
            queue.enqueue(i);
        }
        expect(static_cast<bool>(queue.length() == 32));
    };

    "constructor_power_of_two_capacity"_test = [] {
        // Test that capacity is rounded up to power of 2
        vstd::LockFreeArrayQueue<int> queue(100);
        expect(static_cast<bool>(queue.length() == 0));
        
        // 100 -> rounded to 128 (next power of 2)
        for (int i = 0; i < 128; ++i) {
            queue.enqueue(i);
        }
        expect(static_cast<bool>(queue.length() == 128));
    };

    "move_constructor"_test = [] {
        // Test move constructor
        vstd::LockFreeArrayQueue<int> queue1(32);
        queue1.enqueue(1);
        queue1.enqueue(2);
        queue1.enqueue(3);
        
        vstd::LockFreeArrayQueue<int> queue2(std::move(queue1));
        
        // queue2 should have the elements
        expect(static_cast<bool>(queue2.length() == 3));
        
        auto val = queue2.dequeue();
        expect(static_cast<bool>(val.has_value()));
        expect(static_cast<bool>(val.value() == 1));
    };

    // ============================================
    // Move Assignment Tests
    // ============================================

    "move_assignment"_test = [] {
        // Test move assignment operator
        vstd::LockFreeArrayQueue<int> queue1(32);
        queue1.enqueue(10);
        queue1.enqueue(20);
        
        vstd::LockFreeArrayQueue<int> queue2(32);
        queue2.enqueue(99);
        
        queue2 = std::move(queue1);
        
        // queue2 should now have queue1's elements
        expect(static_cast<bool>(queue2.length() == 2));
        
        auto val = queue2.dequeue();
        expect(static_cast<bool>(val.has_value()));
        expect(static_cast<bool>(val.value() == 10));
    };

    // ============================================
    // Enqueue Tests
    // ============================================

    "enqueue_basic"_test = [] {
        // Test basic enqueue operation
        vstd::LockFreeArrayQueue<int> queue(32);
        
        queue.enqueue(42);
        expect(static_cast<bool>(queue.length() == 1));
        
        queue.enqueue(100);
        expect(static_cast<bool>(queue.length() == 2));
    };

    "enqueue_non_trivial_type"_test = [] {
        // Test enqueue with non-trivial types
        vstd::LockFreeArrayQueue<TestStruct> queue(32);
        
        queue.enqueue(1, "hello");
        queue.enqueue(2, "world");
        
        expect(static_cast<bool>(queue.length() == 2));
        
        auto val = queue.dequeue();
        expect(static_cast<bool>(val.has_value()));
        expect(static_cast<bool>(val->x == 1));
        expect(static_cast<bool>(val->s == "hello"));
    };

    "enqueue_with_auto_resize"_test = [] {
        // Test that enqueue triggers automatic resize when full
        vstd::LockFreeArrayQueue<int> queue(4);  // Small initial capacity
        
        // Enqueue more than initial capacity
        for (int i = 0; i < 100; ++i) {
            queue.enqueue(i);
        }
        
        expect(static_cast<bool>(queue.length() == 100));
        
        // Verify all values
        for (int i = 0; i < 100; ++i) {
            auto val = queue.dequeue();
            expect(static_cast<bool>(val.has_value()));
            expect(static_cast<bool>(val.value() == i));
        }
    };

    "try_push_success"_test = [] {
        // Test successful try_push
        vstd::LockFreeArrayQueue<int> queue(32);
        
        bool result = queue.try_push(42);
        expect(static_cast<bool>(result == true));
        expect(static_cast<bool>(queue.length() == 1));
    };

    // ============================================
    // Dequeue Tests
    // ============================================

    "dequeue_basic"_test = [] {
        // Test basic dequeue operation
        vstd::LockFreeArrayQueue<int> queue(32);
        queue.enqueue(1);
        queue.enqueue(2);
        queue.enqueue(3);
        
        auto val1 = queue.dequeue();
        expect(static_cast<bool>(val1.has_value()));
        expect(static_cast<bool>(val1.value() == 1));
        expect(static_cast<bool>(queue.length() == 2));
        
        auto val2 = queue.dequeue();
        expect(static_cast<bool>(val2.has_value()));
        expect(static_cast<bool>(val2.value() == 2));
        expect(static_cast<bool>(queue.length() == 1));
    };

    "dequeue_empty_queue"_test = [] {
        // Test dequeue from empty queue returns empty optional
        vstd::LockFreeArrayQueue<int> queue(32);
        
        auto val = queue.dequeue();
        expect(static_cast<bool>(!val.has_value()));
        expect(static_cast<bool>(queue.length() == 0));
    };

    "dequeue_fifo_order"_test = [] {
        // Test FIFO (First In First Out) order is maintained
        vstd::LockFreeArrayQueue<int> queue(32);
        
        for (int i = 0; i < 10; ++i) {
            queue.enqueue(i);
        }
        
        for (int i = 0; i < 10; ++i) {
            auto val = queue.dequeue();
            expect(static_cast<bool>(val.has_value()));
            expect(static_cast<bool>(val.value() == i));
        }
    };

    "pop_with_pointer"_test = [] {
        // Test pop(T* ptr) overload
        vstd::LockFreeArrayQueue<int> queue(32);
        queue.enqueue(42);
        queue.enqueue(100);
        
        int value;
        bool result = queue.pop(&value);
        
        expect(static_cast<bool>(result == true));
        expect(static_cast<bool>(value == 42));
        expect(static_cast<bool>(queue.length() == 1));
    };

    "pop_empty_queue"_test = [] {
        // Test pop from empty queue returns false
        vstd::LockFreeArrayQueue<int> queue(32);
        
        int value = 999;  // Initialize with sentinel value
        bool result = queue.pop(&value);
        
        expect(static_cast<bool>(result == false));
        // Note: value is destroyed by pop(), so we don't check it
    };

    "try_pop_success"_test = [] {
        // Test successful try_pop
        vstd::LockFreeArrayQueue<int> queue(32);
        queue.enqueue(42);
        
        auto val = queue.try_pop();
        expect(static_cast<bool>(val.has_value()));
        expect(static_cast<bool>(val.value() == 42));
        expect(static_cast<bool>(queue.length() == 0));
    };

    "try_pop_empty"_test = [] {
        // Test try_pop on empty queue
        vstd::LockFreeArrayQueue<int> queue(32);
        
        auto val = queue.try_pop();
        expect(static_cast<bool>(!val.has_value()));
    };

    // ============================================
    // Capacity and Length Tests
    // ============================================

    "length_tracking"_test = [] {
        // Test that length is correctly tracked
        vstd::LockFreeArrayQueue<int> queue(64);
        
        expect(static_cast<bool>(queue.length() == 0));
        
        // Enqueue and check length
        for (int i = 1; i <= 50; ++i) {
            queue.enqueue(i);
            expect(static_cast<bool>(queue.length() == static_cast<size_t>(i)));
        }
        
        // Dequeue and check length
        for (int i = 50; i >= 1; --i) {
            queue.dequeue();
            expect(static_cast<bool>(queue.length() == static_cast<size_t>(i - 1)));
        }
    };

    "reserve_increase_capacity"_test = [] {
        // Test reserve to increase capacity
        vstd::LockFreeArrayQueue<int> queue(32);
        
        // Add some elements
        for (int i = 0; i < 10; ++i) {
            queue.enqueue(i);
        }
        
        // Reserve more space
        queue.reserve(128);
        
        // Elements should still be there
        expect(static_cast<bool>(queue.length() == 10));
        
        // Add more elements
        for (int i = 10; i < 50; ++i) {
            queue.enqueue(i);
        }
        
        expect(static_cast<bool>(queue.length() == 50));
        
        // Verify order
        for (int i = 0; i < 50; ++i) {
            auto val = queue.dequeue();
            expect(static_cast<bool>(val.has_value()));
            expect(static_cast<bool>(val.value() == i));
        }
    };

    "reserve_no_shrink"_test = [] {
        // Test that reserve doesn't shrink capacity
        vstd::LockFreeArrayQueue<int> queue(128);
        
        for (int i = 0; i < 50; ++i) {
            queue.enqueue(i);
        }
        
        // Try to reserve less than current - should be no-op
        queue.reserve(32);
        
        // All elements should still be there
        expect(static_cast<bool>(queue.length() == 50));
    };

    // ============================================
    // Interleaved Operations Tests
    // ============================================

    "interleaved_enqueue_dequeue"_test = [] {
        // Test interleaved enqueue and dequeue
        vstd::LockFreeArrayQueue<int> queue(16);
        
        // Enqueue and dequeue in mixed pattern
        for (int round = 0; round < 5; ++round) {
            for (int i = 0; i < 10; ++i) {
                queue.enqueue(round * 10 + i);
            }
            
            for (int i = 0; i < 10; ++i) {
                auto val = queue.dequeue();
                expect(static_cast<bool>(val.has_value()));
                expect(static_cast<bool>(val.value() == round * 10 + i));
            }
        }
        
        expect(static_cast<bool>(queue.length() == 0));
    };

    "enqueue_dequeue_around_boundary"_test = [] {
        // Test enqueue/dequeue that wraps around the circular buffer
        vstd::LockFreeArrayQueue<int> queue(8);  // Small capacity for wrap-around
        
        // Fill queue
        for (int i = 0; i < 8; ++i) {
            queue.enqueue(i);
        }
        
        // Dequeue half
        for (int i = 0; i < 4; ++i) {
            auto val = queue.dequeue();
            expect(static_cast<bool>(val.value() == i));
        }
        
        // Enqueue more (should wrap around)
        for (int i = 8; i < 12; ++i) {
            queue.enqueue(i);
        }
        
        // Dequeue remaining (should be in correct order)
        for (int i = 4; i < 12; ++i) {
            auto val = queue.dequeue();
            expect(static_cast<bool>(val.has_value()));
            expect(static_cast<bool>(val.value() == i));
        }
    };

    // ============================================
    // Multi-threaded Tests
    // ============================================

    "multi_threaded_producer_consumer"_test = [] {
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
        
        expect(static_cast<bool>(sum_produced.load() == sum_consumed.load()));
    };

    "multi_threaded_multiple_producers"_test = [] {
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
        
        expect(static_cast<bool>(queue.length() == static_cast<size_t>(num_producers * items_per_producer)));
        expect(static_cast<bool>(total_enqueued.load() == num_producers * items_per_producer));
    };

    // ============================================
    // Edge Cases and Stress Tests
    // ============================================

    "large_number_of_elements"_test = [] {
        // Test with large number of elements
        vstd::LockFreeArrayQueue<int> queue(1024);
        const int count = 100000;
        
        for (int i = 0; i < count; ++i) {
            queue.enqueue(i);
        }
        
        expect(static_cast<bool>(queue.length() == static_cast<size_t>(count)));
        
        for (int i = 0; i < count; ++i) {
            auto val = queue.dequeue();
            expect(static_cast<bool>(val.has_value()));
            expect(static_cast<bool>(val.value() == i));
        }
    };

    "rapid_enqueue_dequeue"_test = [] {
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
        
        expect(static_cast<bool>(queue.length() == 0));
    };

    "destructor_cleanup"_test = [] {
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
        expect(static_cast<bool>(destructor_count.load() >= 10));
    };
