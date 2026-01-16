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
    void wait(const std::chrono::duration<Rep, Period> &duration, luisa::string_view name) {
        std::this_thread::sleep_for(duration);
        if (clk.toc() > 3000) [[unlikely]] {
            clk.tic();
            _waiting_sign(name);
        }
    }
};
}// namespace rbc