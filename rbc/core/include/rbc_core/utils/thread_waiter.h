#pragma once
#include <rbc_config.h>
#include <luisa/core/clock.h>
#include <thread>
namespace rbc {
struct ThreadWaiter {
    luisa::Clock clk;
    ThreadWaiter() = default;
    ~ThreadWaiter() = default;
private:
    RBC_CORE_API void _waiting_sign(luisa::string_view name);
public:
    template<class Rep, class Period>
    void wait(const std::chrono::duration<Rep, Period> &duration, luisa::string_view name, uint64_t interval_ms = 3000) {
        std::this_thread::sleep_for(duration);
        if (clk.toc() > interval_ms) [[unlikely]] {
            clk.tic();
            _waiting_sign(name);
        }
    }
    template<class Rep, class Period, typename Callback>
        requires std::is_invocable_v<Callback>
    void wait(const std::chrono::duration<Rep, Period> &duration, Callback &&callback, uint64_t interval_ms = 3000) {
        std::this_thread::sleep_for(duration);
        if (clk.toc() > interval_ms) [[unlikely]] {
            clk.tic();
            callback();
        }
    }
};
}// namespace rbc