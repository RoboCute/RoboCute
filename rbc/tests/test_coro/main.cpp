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

int main() {
    auto coro = my_coro();
    while (true) {
        coro.resume();
        if (coro.done()) break;
        LUISA_INFO("Sleeping...");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}