#include "test_util.h"
#include <rbc_core/containers/rbc_concurrent_queue.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

TEST_SUITE("core") {
    TEST_CASE("concurrent_queue_basic_enqueue_dequeue") {
        rbc::ConcurrentQueue<int> queue;
        
        // Test basic enqueue
        CHECK(queue.enqueue(42) == true);
        CHECK(queue.enqueue(100) == true);
        CHECK(queue.enqueue(200) == true);
        
        // Test basic dequeue
        int value;
        CHECK(queue.try_dequeue(value) == true);
        CHECK(value == 42);
        
        CHECK(queue.try_dequeue(value) == true);
        CHECK(value == 100);
        
        CHECK(queue.try_dequeue(value) == true);
        CHECK(value == 200);
        
        // Queue should be empty now
        CHECK(queue.try_dequeue(value) == false);
    }

    TEST_CASE("concurrent_queue_move_semantics") {
        rbc::ConcurrentQueue<std::unique_ptr<int>> queue;
        
        auto ptr1 = std::make_unique<int>(42);
        auto ptr2 = std::make_unique<int>(100);
        
        CHECK(queue.enqueue(std::move(ptr1)) == true);
        CHECK(queue.enqueue(std::move(ptr2)) == true);
        CHECK(ptr1 == nullptr); // Should be moved
        CHECK(ptr2 == nullptr); // Should be moved
        
        std::unique_ptr<int> result;
        CHECK(queue.try_dequeue(result) == true);
        CHECK(result != nullptr);
        CHECK(*result == 42);
        
        CHECK(queue.try_dequeue(result) == true);
        CHECK(*result == 100);
    }

    TEST_CASE("concurrent_queue_producer_token") {
        rbc::ConcurrentQueue<int> queue;
        rbc::ConcurrentQueue<int>::producer_token_t producer(queue);
        
        CHECK(producer.valid() == true);
        
        // Enqueue using producer token
        CHECK(queue.enqueue(producer, 42) == true);
        CHECK(queue.enqueue(producer, 100) == true);
        
        int value;
        CHECK(queue.try_dequeue(value) == true);
        CHECK(value == 42);
        
        CHECK(queue.try_dequeue(value) == true);
        CHECK(value == 100);
    }

    TEST_CASE("concurrent_queue_consumer_token") {
        rbc::ConcurrentQueue<int> queue;
        
        // Enqueue some items
        for (int i = 0; i < 10; ++i) {
            queue.enqueue(i);
        }
        
        rbc::ConcurrentQueue<int>::consumer_token_t consumer(queue);
        
        // Dequeue using consumer token
        int value;
        for (int i = 0; i < 10; ++i) {
            CHECK(queue.try_dequeue(consumer, value) == true);
            CHECK(value == i);
        }
        
        CHECK(queue.try_dequeue(consumer, value) == false);
    }

    TEST_CASE("concurrent_queue_bulk_enqueue_dequeue") {
        rbc::ConcurrentQueue<int> queue;
        
        // Bulk enqueue
        std::vector<int> items = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        CHECK(queue.enqueue_bulk(items.begin(), items.size()) == true);
        
        // Bulk dequeue
        std::vector<int> results(10);
        size_t dequeued = queue.try_dequeue_bulk(results.begin(), 10);
        CHECK(dequeued == 10);
        
        for (size_t i = 0; i < 10; ++i) {
            CHECK(results[i] == static_cast<int>(i + 1));
        }
    }

    TEST_CASE("concurrent_queue_bulk_with_tokens") {
        rbc::ConcurrentQueue<int> queue;
        rbc::ConcurrentQueue<int>::producer_token_t producer(queue);
        rbc::ConcurrentQueue<int>::consumer_token_t consumer(queue);
        
        // Bulk enqueue with producer token
        std::vector<int> items = {10, 20, 30, 40, 50};
        CHECK(queue.enqueue_bulk(producer, items.begin(), items.size()) == true);
        
        // Bulk dequeue with consumer token
        std::vector<int> results(5);
        size_t dequeued = queue.try_dequeue_bulk(consumer, results.begin(), 5);
        CHECK(dequeued == 5);
        
        for (size_t i = 0; i < 5; ++i) {
            CHECK(results[i] == static_cast<int>((i + 1) * 10));
        }
    }

    TEST_CASE("concurrent_queue_size_approx") {
        rbc::ConcurrentQueue<int> queue;
        
        CHECK(queue.size_approx() == 0);
        
        for (int i = 0; i < 100; ++i) {
            queue.enqueue(i);
        }
        
        // Size is approximate, so we check it's at least close
        size_t size = queue.size_approx();
        CHECK(size >= 90); // Allow some variance
        CHECK(size <= 110);
        
        // Dequeue some items
        int value;
        for (int i = 0; i < 50; ++i) {
            queue.try_dequeue(value);
        }
        
        size = queue.size_approx();
        CHECK(size >= 40);
        CHECK(size <= 60);
    }

    TEST_CASE("concurrent_queue_multithreaded_producer") {
        rbc::ConcurrentQueue<int> queue;
        const int num_threads = 4;
        const int items_per_thread = 100;
        std::atomic<int> enqueued_count(0);
        
        std::vector<std::thread> threads;
        threads.reserve(num_threads);
        
        // Create producer threads
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&queue, &enqueued_count, t, items_per_thread]() {
                for (int i = 0; i < items_per_thread; ++i) {
                    if (queue.enqueue(t * items_per_thread + i)) {
                        enqueued_count.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }
        
        // Wait for all producers
        for (auto &thread : threads) {
            thread.join();
        }
        
        CHECK(enqueued_count.load() == num_threads * items_per_thread);
        
        // Verify all items can be dequeued
        std::vector<bool> received(num_threads * items_per_thread, false);
        int value;
        int dequeued = 0;
        
        while (queue.try_dequeue(value)) {
            CHECK(value >= 0);
            CHECK(value < num_threads * items_per_thread);
            received[value] = true;
            ++dequeued;
        }
        
        CHECK(dequeued == num_threads * items_per_thread);
        
        // Verify all items were received
        for (size_t i = 0; i < received.size(); ++i) {
            CHECK(received[i] == true);
        }
    }

    TEST_CASE("concurrent_queue_multithreaded_consumer") {
        rbc::ConcurrentQueue<int> queue;
        const int num_items = 1000;
        const int num_threads = 4;
        
        // Enqueue all items first
        for (int i = 0; i < num_items; ++i) {
            queue.enqueue(i);
        }
        
        std::atomic<int> dequeued_count(0);
        std::vector<std::thread> threads;
        threads.reserve(num_threads);
        
        // Create consumer threads
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&queue, &dequeued_count]() {
                int value;
                while (queue.try_dequeue(value)) {
                    dequeued_count.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        
        // Wait for all consumers
        for (auto &thread : threads) {
            thread.join();
        }
        
        CHECK(dequeued_count.load() == num_items);
    }

    TEST_CASE("concurrent_queue_producer_consumer") {
        rbc::ConcurrentQueue<int> queue;
        const int num_producers = 2;
        const int num_consumers = 2;
        const int items_per_producer = 500;
        const int total_items = num_producers * items_per_producer;
        
        std::atomic<int> enqueued_count(0);
        std::atomic<int> dequeued_count(0);
        
        std::vector<std::thread> producer_threads;
        std::vector<std::thread> consumer_threads;
        producer_threads.reserve(num_producers);
        consumer_threads.reserve(num_consumers);
        
        // Create producer threads
        for (int t = 0; t < num_producers; ++t) {
            producer_threads.emplace_back([&queue, &enqueued_count, t, items_per_producer]() {
                for (int i = 0; i < items_per_producer; ++i) {
                    if (queue.enqueue(t * items_per_producer + i)) {
                        enqueued_count.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }
        
        // Create consumer threads
        for (int t = 0; t < num_consumers; ++t) {
            consumer_threads.emplace_back([&queue, &dequeued_count, total_items]() {
                int value;
                while (dequeued_count.load() < total_items) {
                    if (queue.try_dequeue(value)) {
                        dequeued_count.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        std::this_thread::sleep_for(std::chrono::microseconds(10));
                    }
                }
            });
        }
        
        // Wait for all producers
        for (auto &thread : producer_threads) {
            thread.join();
        }
        
        // Wait for all consumers
        for (auto &thread : consumer_threads) {
            thread.join();
        }
        
        CHECK(enqueued_count.load() == total_items);
        CHECK(dequeued_count.load() == total_items);
    }

    TEST_CASE("concurrent_queue_try_enqueue") {
        rbc::ConcurrentQueue<int> queue(10); // Small capacity
        
        // Try enqueue should succeed when there's space
        CHECK(queue.try_enqueue(1) == true);
        CHECK(queue.try_enqueue(2) == true);
        
        int value;
        CHECK(queue.try_dequeue(value) == true);
        CHECK(value == 1);
        
        CHECK(queue.try_dequeue(value) == true);
        CHECK(value == 2);
    }

    TEST_CASE("concurrent_queue_empty_queue") {
        rbc::ConcurrentQueue<int> queue;
        
        int value;
        CHECK(queue.try_dequeue(value) == false);
        CHECK(queue.size_approx() == 0);
    }

    TEST_CASE("concurrent_queue_custom_type") {
        struct TestStruct {
            int a;
            float b;
            std::string c;
            
            TestStruct() : a(0), b(0.0f), c() {}
            TestStruct(int a, float b, const std::string &c) : a(a), b(b), c(c) {}
        };
        
        rbc::ConcurrentQueue<TestStruct> queue;
        
        TestStruct item1(42, 3.14f, "test1");
        TestStruct item2(100, 2.71f, "test2");
        
        CHECK(queue.enqueue(item1) == true);
        CHECK(queue.enqueue(std::move(item2)) == true);
        
        TestStruct result;
        CHECK(queue.try_dequeue(result) == true);
        CHECK(result.a == 42);
        CHECK(result.b == 3.14f);
        CHECK(result.c == "test1");
        
        CHECK(queue.try_dequeue(result) == true);
        CHECK(result.a == 100);
        CHECK(result.b == 2.71f);
        CHECK(result.c == "test2");
    }

    TEST_CASE("concurrent_queue_is_lock_free") {
        rbc::ConcurrentQueue<int> queue;
        
        // Check if the underlying atomic operations are lock-free
        // This is platform-dependent, but should be true on most modern platforms
        bool lock_free = rbc::ConcurrentQueue<int>::is_lock_free();
        // Just verify the method exists and can be called
        (void)lock_free;
    }
}

