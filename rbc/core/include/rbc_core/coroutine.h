#pragma once
#include <rbc_config.h>
#include <coroutine>
#include <luisa/vstl/meta_lib.h>
#include <luisa/core/logging.h>

namespace rbc::coro {
struct promise;
// Coro from cppref
struct coroutine {
    using promise_type = rbc::coro::promise;
    using base_type = std::coroutine_handle<rbc::coro::promise>;
private:
    base_type _base;
    bool _own{};
public:
    coroutine() = default;
    coroutine(base_type &&rhs) : _base(rhs), _own{true} {}
    coroutine(coroutine const &) = delete;
    coroutine(coroutine &&rhs)
        : _base(rhs._base),
          _own(rhs._own) {
        rhs._own = false;
        rhs._base = {};
    }
    static auto from_promise(promise &prom) {
        return base_type::from_promise(prom);
    }
    coroutine &operator=(coroutine const &rhs) = delete;
    coroutine &operator=(coroutine &&rhs) {
        vstd::reset(*this, std::move(rhs));
        return *this;
    }
    promise &promise() {
        LUISA_DEBUG_ASSERT(_own, "Coroutine already disposed.");
        return _base.promise();
    }
    void resume() {
        LUISA_DEBUG_ASSERT(_own, "Coroutine already disposed.");
        _base.resume();
    }
    bool done() {
        LUISA_DEBUG_ASSERT(_own, "Coroutine already disposed.");
        return _base.done();
    }
    void destroy() {
        if (!_own) return;
        _base.destroy();
        _own = false;
        _base = {};
    }
    ~coroutine() {
        if (_own) {
            _base.destroy();
        }
    }
};

struct promise {
    coroutine get_return_object() { return {coroutine::from_promise(*this)}; }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() {}
};

struct awaitable {
private:
    bool _ready;
public:
    awaitable(bool ready = false) : _ready{ready} {}
    bool await_ready() { return _ready; }
    void await_suspend(std::coroutine_handle<> h) {
    }
    void await_resume() {}
};
}// namespace rbc::coro
#define RBC_AwaitCoroutine(coro_expr)          \
    {                                          \
        auto &&cc = (coro_expr);               \
        while (!cc.done()) {                   \
            co_await ::rbc::coro::awaitable{}; \
            cc.resume();                       \
        }                                      \
    }