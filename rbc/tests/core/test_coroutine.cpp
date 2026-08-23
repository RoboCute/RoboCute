#include "rbc_test.hpp"
#include <rbc_core/coroutine.h>
#include <memory>

namespace rbc::test {
suite<"Core|Coroutine"> CoreCoroutineTestSuite = [] {

    "coroutine capture keeps a move-only closure alive"_test = [] {
        int result = 0;
        auto task = rbc::capture(
            [value = std::make_unique<int>(21), &result]() -> rbc::coroutine {
                result = *value;
                co_await std::suspend_always{};
                result += *value;
            });

        expect(static_cast<bool>(result == 0));
        expect(!static_cast<bool>(task.done()));

        auto moved_task = std::move(task);
        expect(static_cast<bool>(task.done()));

        moved_task.resume();
        expect(static_cast<bool>(result == 21));
        expect(!static_cast<bool>(moved_task.done()));

        moved_task.resume();
        expect(static_cast<bool>(result == 42));
        expect(static_cast<bool>(moved_task.done()));
    };

    "coroutine capture polls nested awaitables"_test = [] {
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
        expect(static_cast<bool>(result == 0));
        expect(!static_cast<bool>(task.done()));

        task.resume();
        expect(static_cast<bool>(result == 0));
        expect(!static_cast<bool>(task.done()));

        ready = true;
        task.resume();
        expect(static_cast<bool>(result == 42));
        expect(static_cast<bool>(task.done()));
    };

    "coroutine capture completes without an extra resume"_test = [] {
        int result = 0;
        auto task = rbc::capture([&result]() -> rbc::coroutine {
            result = 42;
            co_return;
        });

        task.resume();
        expect(static_cast<bool>(result == 42));
        expect(static_cast<bool>(task.done()));
    };
}; // suite

} // namespace rbc::test
