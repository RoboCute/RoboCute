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
coro::coroutine my_internal_coro() {
    LUISA_INFO("Internal 1");
    co_await coro::awaitable{};
    LUISA_INFO("Internal 2");
    co_await coro::awaitable{};
    LUISA_INFO("Internal 3");
}
coro::coroutine my_coro() {
    TestLifeTime t;
    LUISA_INFO("1");
    RBC_AwaitCoroutine(my_internal_coro());
    co_await coro::awaitable{};
    LUISA_INFO("2");
    co_await coro::awaitable{};
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