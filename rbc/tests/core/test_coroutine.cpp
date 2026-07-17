#include "test_util.h"
#include <rbc_core/coroutine.h>
#include <memory>

TEST_SUITE("core") {
    TEST_CASE("coroutine capture keeps a move-only closure alive") {
        int result = 0;
        auto task = rbc::capture(
            [value = std::make_unique<int>(21), &result]() -> rbc::coroutine {
                result = *value;
                co_await std::suspend_always{};
                result += *value;
            });

        CHECK(result == 0);
        CHECK_FALSE(task.done());

        auto moved_task = std::move(task);
        CHECK(task.done());

        moved_task.resume();
        CHECK(result == 21);
        CHECK_FALSE(moved_task.done());

        moved_task.resume();
        CHECK(result == 42);
        CHECK(moved_task.done());
    }

    TEST_CASE("coroutine capture polls nested awaitables") {
        bool ready = false;
        int result = 0;
        auto task = rbc::capture(
            [](bool *ready, int *result) -> rbc::coroutine {
                co_await rbc::awaitable([ready] { return *ready; });
                *result = 42;
            },
            &ready,
            &result);

        task.resume();
        CHECK(result == 0);
        CHECK_FALSE(task.done());

        task.resume();
        CHECK(result == 0);
        CHECK_FALSE(task.done());

        ready = true;
        task.resume();
        CHECK(result == 42);
        CHECK(task.done());
    }

    TEST_CASE("coroutine capture completes without an extra resume") {
        int result = 0;
        auto task = rbc::capture([&result]() -> rbc::coroutine {
            result = 42;
            co_return;
        });

        task.resume();
        CHECK(result == 42);
        CHECK(task.done());
    }
}
