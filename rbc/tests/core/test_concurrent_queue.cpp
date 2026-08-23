#include "rbc_test.hpp"
#include <rbc_core/containers/rbc_concurrent_queue.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

namespace rbc::test {
suite<"Core|ConcurrentQueue"> CoreConcurrentQueueTestSuite = [] {

    "concurrent_queue_basic_enqueue_dequeue"_test = [] {
        rbc::ConcurrentQueue<int> queue;
        
        // Test basic enqueue
        expect(static_cast<bool>(queue.enqueue(42) == true));
        expect(static_cast<bool>(queue.enqueue(100) == true));
        expect(static_cast<bool>(queue.enqueue(200) == true));
        
        // Test basic dequeue
        int value;
        expect(static_cast<bool>(queue.try_dequeue(value) == true));
        expect(static_cast<bool>(value == 42));
        
        expect(static_cast<bool>(queue.try_dequeue(value) == true));
        expect(static_cast<bool>(value == 100));
        
        expect(static_cast<bool>(queue.try_dequeue(value) == true));
        expect(static_cast<bool>(value == 200));
        
        // Queue should be empty now
        expect(static_cast<bool>(queue.try_dequeue(value) == false));
    };

    "concurrent_queue_move_semantics"_test = [] {
        rbc::ConcurrentQueue<std::unique_ptr<int>> queue;
        
        auto ptr1 = std::make_unique<int>(42);
        auto ptr2 = std::make_unique<int>(100);
        
        expect(static_cast<bool>(queue.enqueue(std::move(ptr1)) == true));
        expect(static_cast<bool>(queue.enqueue(std::move(ptr2)) == true));
        expect(static_cast<bool>(ptr1 == nullptr)); // Should be moved
        expect(static_cast<bool>(ptr2 == nullptr)); // Should be moved
        
        std::unique_ptr<int> result;
        expect(static_cast<bool>(queue.try_dequeue(result) == true));
        expect(static_cast<bool>(result != nullptr));
        expect(static_cast<bool>(*result == 42));
        
        expect(static_cast<bool>(queue.try_dequeue(result) == true));
        expect(static_cast<bool>(*result == 100));
    };

    "concurrent_queue_producer_token"_test = [] {
        rbc::ConcurrentQueue<int> queue;
        rbc::ConcurrentQueue<int>::producer_token_t producer(queue);
        
        expect(static_cast<bool>(producer.valid() == true));
        
        // Enqueue using producer token
        expect(static_cast<bool>(queue.enqueue(producer, 42) == true));
        expect(static_cast<bool>(queue.enqueue(producer, 100) == true));
        
        int value;
        expect(static_cast<bool>(queue.try_dequeue(value) == true));
        expect(static_cast<bool>(value == 42));
        
        expect(static_cast<bool>(queue.try_dequeue(value) == true));
        expect(static_cast<bool>(value == 100));
    };

    "concurrent_queue_consumer_token"_test = [] {
        rbc::ConcurrentQueue<int> queue;
        
        // Enqueue some items
        for (int i = 0; i < 10; ++i) {
            queue.enqueue(i);
        }
        
        rbc::ConcurrentQueue<int>::consumer_token_t consumer(queue);
        
        // Dequeue using consumer token
        int value;
        for (int i = 0; i < 10; ++i) {
            expect(static_cast<bool>(queue.try_dequeue(consumer, value) == true));
            expect(static_cast<bool>(value == i));
        }
        
        expect(static_cast<bool>(queue.try_dequeue(consumer, value) == false));
    };

    "concurrent_queue_bulk_enqueue_dequeue"_test = [] {
        rbc::ConcurrentQueue<int> queue;
        
        // Bulk enqueue
        std::vector<int> items = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        expect(static_cast<bool>(queue.enqueue_bulk(items.begin(), items.size()) == true));
        
        // Bulk dequeue
        std::vector<int> results(10);
        size_t dequeued = queue.try_dequeue_bulk(results.begin(), 10);
        expect(static_cast<bool>(dequeued == 10));
        
        for (size_t i = 0; i < 10; ++i) {
            expect(static_cast<bool>(results[i] == static_cast<int>(i + 1)));
        }
    };

    "concurrent_queue_bulk_with_tokens"_test = [] {
        rbc::ConcurrentQueue<int> queue;
        rbc::ConcurrentQueue<int>::producer_token_t producer(queue);
        rbc::ConcurrentQueue<int>::consumer_token_t consumer(queue);
        
        // Bulk enqueue with producer token
        std::vector<int> items = {10, 20, 30, 40, 50};
        expect(static_cast<bool>(queue.enqueue_bulk(producer, items.begin(), items.size()) == true));
        
        // Bulk dequeue with consumer token
        std::vector<int> results(5);
        size_t dequeued = queue.try_dequeue_bulk(consumer, results.begin(), 5);
        expect(static_cast<bool>(dequeued == 5));
        
        for (size_t i = 0; i < 5; ++i) {
            expect(static_cast<bool>(results[i] == static_cast<int>((i + 1) * 10)));
        }
    };

    "concurrent_queue_size_approx"_test = [] {
        rbc::ConcurrentQueue<int> queue;
        
        expect(static_cast<bool>(queue.size_approx() == 0));
        
        for (int i = 0; i < 100; ++i) {
            queue.enqueue(i);
        }
        
        // Size is approximate, so we check it's at least close
        size_t size = queue.size_approx();
        expect(static_cast<bool>(size >= 90)); // Allow some variance
        expect(static_cast<bool>(size <= 110));
        
        // Dequeue some items
        int value;
        for (int i = 0; i < 50; ++i) {
            queue.try_dequeue(value);
        }
        
        size = queue.size_approx();
        expect(static_cast<bool>(size >= 40));
        expect(static_cast<bool>(size <= 60));
    };

    "concurrent_queue_multithreaded_producer"_test = [] {
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
        
        expect(static_cast<bool>(enqueued_count.load() == num_threads * items_per_thread));
        
        // Verify all items can be dequeued
        std::vector<bool> received(num_threads * items_per_thread, false);
        int value;
        int dequeued = 0;
        
        while (queue.try_dequeue(value)) {
            expect(static_cast<bool>(value >= 0));
            expect(static_cast<bool>(value < num_threads * items_per_thread));
            received[value] = true;
            ++dequeued;
        }
        
        expect(static_cast<bool>(dequeued == num_threads * items_per_thread));
        
        // Verify all items were received
        for (size_t i = 0; i < received.size(); ++i) {
            expect(static_cast<bool>(received[i] == true));
        }
    };

    "concurrent_queue_multithreaded_consumer"_test = [] {
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
        
        expect(static_cast<bool>(dequeued_count.load() == num_items));
    };

    "concurrent_queue_producer_consumer"_test = [] {
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
        
        expect(static_cast<bool>(enqueued_count.load() == total_items));
        expect(static_cast<bool>(dequeued_count.load() == total_items));
    };

    "concurrent_queue_try_enqueue"_test = [] {
        rbc::ConcurrentQueue<int> queue(10); // Small capacity
        
        // Try enqueue should succeed when there's space
        expect(static_cast<bool>(queue.try_enqueue(1) == true));
        expect(static_cast<bool>(queue.try_enqueue(2) == true));
        
        int value;
        expect(static_cast<bool>(queue.try_dequeue(value) == true));
        expect(static_cast<bool>(value == 1));
        
        expect(static_cast<bool>(queue.try_dequeue(value) == true));
        expect(static_cast<bool>(value == 2));
    };

    "concurrent_queue_empty_queue"_test = [] {
        rbc::ConcurrentQueue<int> queue;
        
        int value;
        expect(static_cast<bool>(queue.try_dequeue(value) == false));
        expect(static_cast<bool>(queue.size_approx() == 0));
    };

    "concurrent_queue_custom_type"_test = [] {
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
        
        expect(static_cast<bool>(queue.enqueue(item1) == true));
        expect(static_cast<bool>(queue.enqueue(std::move(item2)) == true));
        
        TestStruct result;
        expect(static_cast<bool>(queue.try_dequeue(result) == true));
        expect(static_cast<bool>(result.a == 42));
        expect(static_cast<bool>(result.b == 3.14f));
        expect(static_cast<bool>(result.c == "test1"));
        
        expect(static_cast<bool>(queue.try_dequeue(result) == true));
        expect(static_cast<bool>(result.a == 100));
        expect(static_cast<bool>(result.b == 2.71f));
        expect(static_cast<bool>(result.c == "test2"));
    };

    "concurrent_queue_is_lock_free"_test = [] {
        rbc::ConcurrentQueue<int> queue;
        
        // Check if the underlying atomic operations are lock-free
        // This is platform-dependent, but should be true on most modern platforms
        bool lock_free = rbc::ConcurrentQueue<int>::is_lock_free();
        // Just verify the method exists and can be called
        (void)lock_free;
    };

}; // suite

} // namespace rbc::test
