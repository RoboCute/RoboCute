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
coro::coroutine my_coro() {
    TestLifeTime t;
    LUISA_INFO("1");
    co_await coro::awaitable{};
    LUISA_INFO("2");
    co_await coro::awaitable{};
    LUISA_INFO("3");
}

coro::generator<int> my_coro_with_return() {
    TestLifeTime t;
    co_yield 1;
    co_yield 2;
    co_yield 3;
    LUISA_INFO("Yield finished ...");
}

int main() {
    auto return_coro = my_coro_with_return();
    while (true) {
        auto ret_value = return_coro();
        if (ret_value)
            LUISA_INFO("Get value {}", *ret_value);
        else
            break;
        LUISA_INFO("Sleeping...");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    auto coro = my_coro();
    while (true) {
        coro.resume();
        if (coro.done()) break;
        LUISA_INFO("Sleeping...");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}