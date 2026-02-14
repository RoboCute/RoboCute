#include <rbc_core/coroutine.h>
#include <luisa/core/logging.h>
using namespace rbc;
struct TestLifeTime {
    TestLifeTime() {
        LUISA_INFO("coroutine life-time start.");
    }
    ~TestLifeTime() {
        LUISA_INFO("coroutine life-time end.\n");
    }
};
struct WaitThreeTimes : rbc::i_awaitable<WaitThreeTimes> {
    int time = 0;
    ~WaitThreeTimes() {
    }
    bool await_ready() {
        time++;
        return time > 3;
    }
};

coroutine my_coro() {
    TestLifeTime t;
    LUISA_INFO("1");
    co_await std::suspend_always{};
    LUISA_INFO("2");
    co_await WaitThreeTimes{};
    LUISA_INFO("3");
}

// Example 2: Coroutine with custom awaitable using lambda
struct WaitForCondition : rbc::i_awaitable<WaitForCondition> {
    int& counter;
    int target;
    WaitForCondition(int& c, int t) : counter(c), target(t) {}
    bool await_ready() {
        return counter >= target;
    }
};

coroutine counter_coro(int& counter) {
    LUISA_INFO("Counter coroutine started");
    co_await std::suspend_always{};
    counter++;
    LUISA_INFO("Counter incremented to {}", counter);
    co_await WaitForCondition{counter, 3};
    LUISA_INFO("Counter reached target");
}

// Example 3: Chained coroutines - one coroutine resumes another
struct CoroAwaitable : rbc::i_awaitable<CoroAwaitable> {
    coroutine& target;
    CoroAwaitable(coroutine& c) : target(c) {}
    bool await_ready() {
        if (target.done()) return true;
        target.resume();
        return target.done();
    }
};

coroutine inner_coro(int id) {
    LUISA_INFO("Inner coroutine {} started", id);
    co_await std::suspend_always{};
    LUISA_INFO("Inner coroutine {} step 1", id);
    co_await std::suspend_always{};
    LUISA_INFO("Inner coroutine {} completed", id);
}

coroutine outer_coro(coroutine& inner) {
    LUISA_INFO("Outer coroutine starting");
    co_await CoroAwaitable{inner};
    LUISA_INFO("Outer coroutine finished waiting for inner");
}

// Example 4: Coroutine with multiple sequential suspend points using loop
coroutine sequential_coro() {
    for (int i = 0; i < 5; ++i) {
        LUISA_INFO("Sequential suspend point {}", i);
        co_await std::suspend_always{};
    }
    LUISA_INFO("Sequential coroutine done");
}

// Example 5: Coroutine using awaitable helper with lambda
coroutine lambda_awaitable_coro() {
    int step = 0;
    LUISA_INFO("Lambda awaitable coroutine started");
    
    // Use the awaitable helper with a lambda
    co_await rbc::awaitable([&step]() -> bool {
        step++;
        LUISA_INFO("Lambda check step {}", step);
        return step >= 3;
    });
    
    LUISA_INFO("Lambda awaitable coroutine completed after {} steps", step);
}

// Example 6: Multiple coroutines running interleaved
void run_interleaved_coroutines() {
    LUISA_INFO("=== Running interleaved coroutines ===");
    auto coro_a = []() -> coroutine {
        for (int i = 0; i < 3; ++i) {
            LUISA_INFO("Coro A - step {}", i);
            co_await std::suspend_always{};
        }
    }();
    
    auto coro_b = []() -> coroutine {
        for (int i = 0; i < 3; ++i) {
            LUISA_INFO("Coro B - step {}", i);
            co_await std::suspend_always{};
        }
    }();
    
    // Interleave execution
    while (!coro_a.done() || !coro_b.done()) {
        if (!coro_a.done()) {
            LUISA_INFO("[Resume A]");
            coro_a.resume();
        }
        if (!coro_b.done()) {
            LUISA_INFO("[Resume B]");
            coro_b.resume();
        }
    }
    LUISA_INFO("=== Interleaved coroutines completed ===");
}

int main() {
    auto coro = my_coro();
    while (true) {
        coro.resume();
        if (coro.done()) break;
        LUISA_INFO("Sleeping...");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    LUISA_INFO("\n=== Example 2: Counter Coroutine ===");
    int counter = 0;
    auto coro2 = counter_coro(counter);
    while (!coro2.done()) {
        coro2.resume();
        if (!coro2.done()) {
            counter++;
            LUISA_INFO("Main: counter incremented to {}", counter);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    LUISA_INFO("\n=== Example 3: Chained Coroutines ===");
    auto inner = inner_coro(1);
    auto outer = outer_coro(inner);
    while (!outer.done()) {
        outer.resume();
        if (!outer.done()) {
            LUISA_INFO("Waiting for outer coroutine...");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    LUISA_INFO("\n=== Example 4: Sequential Coroutine ===");
    auto seq_coro = sequential_coro();
    while (!seq_coro.done()) {
        seq_coro.resume();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    LUISA_INFO("\n=== Example 5: Lambda Awaitable Coroutine ===");
    auto lambda_coro = lambda_awaitable_coro();
    while (!lambda_coro.done()) {
        lambda_coro.resume();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    LUISA_INFO("\n=== Example 6: Interleaved Coroutines ===");
    run_interleaved_coroutines();
    
    LUISA_INFO("\nAll coroutine examples completed!");
}
