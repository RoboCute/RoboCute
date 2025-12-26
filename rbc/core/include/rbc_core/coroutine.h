#pragma once
#include <rbc_config.h>
#include <coroutine>
#include <memory>
#include <luisa/core/stl/type_traits.h>

namespace rbc {

struct promise;
// Coro from cppref
struct coroutine {
    friend struct promise;
    using promise_type = rbc::promise;
    using base_type = std::coroutine_handle<rbc::promise>;
private:
    base_type _base;
    bool _own{};
public:
    coroutine() = default;
    coroutine(base_type &&rhs) : _base(rhs), _own{true} {}
    coroutine(coroutine const &) = delete;
    RBC_CORE_API coroutine(coroutine &&rhs);
    static auto from_promise(promise &prom) {
        return base_type::from_promise(prom);
    }
    coroutine &operator=(coroutine const &rhs) = delete;
    coroutine &operator=(coroutine &&rhs) {
        std::destroy_at(this);
        std::construct_at(this, std::move(rhs));
        return *this;
    }
    RBC_CORE_API void resume();
    RBC_CORE_API bool done();
    RBC_CORE_API void destroy();
    RBC_CORE_API ~coroutine();
};

struct promise {
    friend struct coroutine;
    promise() = default;
    ~promise() = default;
    coroutine get_return_object() {
        return {coroutine::from_promise(*this)};
    }
    std::suspend_always initial_suspend() noexcept {
        return {};
    }
    std::suspend_always final_suspend() noexcept {
        return {};
    }
    void return_void() {
    }
    void unhandled_exception() {}
    promise(promise const &) = delete;
    promise(promise &&) = delete;
    template<typename T>
    void set_callable(T *t) {
        _awaitable_ptr = t;
        _awaitable_func = +[](void *ptr) {
            return static_cast<T *>(ptr)->await_ready();
        };
    }
private:
    void *_awaitable_ptr{};
    bool (*_awaitable_func)(void *){};
};
template<typename T>
struct i_awaitable {
    i_awaitable() = default;
    ~i_awaitable() = default;
    i_awaitable(i_awaitable const &) = delete;
    i_awaitable(i_awaitable &&) = delete;
    void await_suspend(std::coroutine_handle<rbc::promise> h) {
        h.promise().set_callable(static_cast<T *>(this));
    }
    void await_resume() {}
};
}// namespace rbc